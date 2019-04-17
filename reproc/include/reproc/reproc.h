#pragma once

#include <reproc/error.h>
#include <reproc/export.h>

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*! Used to store information about a child process. We define `reproc_type` in
the header file so it can be allocated on the stack but its internals are prone
to change and should **NOT** be depended on. */
struct reproc_type {
  // On POSIX systems, we can't wait again on the same process after
  // successfully waiting once so we store the result.
  bool running;
  unsigned int exit_status;
#if defined(_WIN32)
  // unsigned long = DWORD
  unsigned long id;
  // void * = HANDLE
  void *handle;
  void *in;
  void *out;
  void *err;
#else
  int id;
  int in;
  int out;
  int err;
#endif
};

// We can't name the struct `reproc` because reproc++'s namespace is already
// named `reproc`. `reproc_t` can't be used either because the _t suffix is
// reserved by POSIX so we fall back to `reproc_type` instead.
typedef struct reproc_type reproc_type;

/*! Stream identifiers used to indicate which stream to act on. */
typedef enum {
  /*! stdin */
  REPROC_IN = 0,
  /*! stdout */
  REPROC_OUT = 1,
  /*! stderr */
  REPROC_ERR = 2
} REPROC_STREAM;

/*! Tells a function that takes a timeout value to wait indefinitely. */
REPROC_EXPORT extern const unsigned int REPROC_INFINITE;

/*!
Starts the process specified by `argv` in the given working directory and
redirects its input, output and error streams.

If this function does not return an error the child process will have started
running and can be inspected using the operating system's tools for process
inspection (e.g. ps on Linux).

Every successful call to this function should be followed by a successful call
to `reproc_wait` or `reproc_stop` and a call to `reproc_destroy`. If an error
occurs during `reproc_start` all allocated resources are cleaned up before
`reproc_start` returns and no further action is required.

`argv` is an array of UTF-8 encoded, null terminated strings that specifies the
program to execute along with its arguments. It has the following layout:

- The first element indicates the executable to run as a child process. This can
be an absolute path, a path relative to the working directory of the parent
process or the name of an executable located in the PATH. It cannot be `NULL`.
- The following elements indicate the whitespace delimited arguments passed to
the executable. None of these elements can be `NULL`.
- The final element must be `NULL`.

Example: ["cmake", "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release", `NULL`]

`argc` specifies the amount of elements in `argv`. It must NOT count the `NULL`
value at the end of the array.

Example: if `argv` is `["cmake", "--help", NULL]` then `argc` is 2.

`working_directory` specifies the working directory for the child process. If it
is `NULL`, the child process runs in the same directory as the parent process.

NOTE: Indicating the program to run with a path relative to the working
directory in `argv` with a custom `working_directory` results in different
behaviour on Windows, MacOS and Linux. On Windows and MacOS, the path is
relative to the working directory of the parent process. On Linux, the path is
relative to the working directory of the child process. To avoid issues,
convert the path to an absolute path before passing it to `reproc_start` or use
a path relative to the PATH environment variable when using a custom working
directory.

Possible errors:
- `REPROC_PIPE_LIMIT_REACHED`
- `REPROC_PROCESS_LIMIT_REACHED`
- `REPROC_NOT_ENOUGH_MEMORY`
- `REPROC_INVALID_UNICODE`
- `REPROC_PERMISSION_DENIED`
- `REPROC_SYMLINK_LOOP`
- `REPROC_FILE_NOT_FOUND`
- `REPROC_INTERRUPTED`
*/
REPROC_EXPORT REPROC_ERROR reproc_start(reproc_type *process,
                                        int argc,
                                        const char *const *argv,
                                        const char *working_directory);

/*!
Reads up to `size` bytes from the child process stream indicated by `stream`
(cannot be `REPROC_IN`) and stores them them in `buffer`. The amount of bytes
read is stored in `bytes_read`.

Possible errors:
- `REPROC_STREAM_CLOSED`
- `REPROC_INTERRUPTED`

Assuming no other errors occur this function will return `REPROC_SUCCESS` until
the stream is closed and all remaining data has been read. This allows the
function to be used as follows to read all data from a child process' stdout
stream (`std::string` is used for brevity):

```c++
#define BUFFER_SIZE 1024

...

char buffer[BUFFER_SIZE];
std::string output{};

while (true) {
  unsigned int bytes_read = 0;
  error = reproc_read(process, REPROC_OUT, buffer, BUFFER_SIZE, &bytes_read);
  if (error) { break; }

  output.append(buffer, bytes_read);
}

if (error != REPROC_STREAM_CLOSED) { return error; }

// Do something with the output
```

Remember that this function reads bytes and not strings. To use `buffer` as a
null terminated string, A null terminator has to be added after reading. This is
easily accomplished as long as we make sure to leave space for a null terminator
when reading:

```c++
unsigned int bytes_read = 0;
error = reproc_read(process, REPROC_OUT, buffer, BUFFER_SIZE - 1, &bytes_read);
//                                               ^^^^^^^^^^^^^^^
if (error) { return error; }

buffer[bytes_read] = '\0'; // Add null terminator
```
*/
REPROC_EXPORT REPROC_ERROR reproc_read(reproc_type *process,
                                       REPROC_STREAM stream,
                                       void *buffer,
                                       unsigned int size,
                                       unsigned int *bytes_read);

/*!
Calls `reproc_read` on `stream` until `parser` returns false or an error occurs.
`parser` receives the output after each read, along with `context`.

`parser` is always called once with the empty string to give the parser the
chance to process all output from the previous call to `reproc_parse` one by
one.

See `reproc_read` for a list of possible errors.
*/
REPROC_EXPORT REPROC_ERROR reproc_parse(reproc_type *process,
                                        REPROC_STREAM stream,
                                        bool (*parser)(void *context,
                                                       const char *buffer,
                                                       unsigned int size),
                                        void *context);

/*!
Calls `reproc_read` on `stream` until it is closed, `sink` returns false or an
error occurs. `sink` receives the output after each read, along with `context`.

Note that this method does not report `stream` being closed as an error. This is
also the main difference with `reproc_parse`.

See `reproc_read` for a list of possible errors (except for
`REPROC_STREAM_CLOSED`).
*/
REPROC_EXPORT REPROC_ERROR reproc_drain(reproc_type *process,
                                        REPROC_STREAM stream,
                                        bool (*sink)(void *context,
                                                     const char *buffer,
                                                     unsigned int size),
                                        void *context);

/*!
Writes up to `to_write` bytes from `buffer` to the standard input (stdin) of
the child process and stores the amount of bytes written in `bytes_written`.

For pipes, the `write` system call on both Windows and POSIX platforms will
block until the requested amount of bytes have been written to the pipe so this
function should only rarely succeed without writing the full amount of bytes
requested.

(POSIX) By default, writing to a closed stdin pipe terminates the parent process
with the `SIGPIPE` signal. `reproc_write` will only return
`REPROC_STREAM_CLOSED` if this signal is ignored by the parent process.

Possible errors:
- `REPROC_STREAM_CLOSED`
- `REPROC_INTERRUPTED`
- `REPROC_PARTIAL_WRITE`
*/
REPROC_EXPORT REPROC_ERROR reproc_write(reproc_type *process,
                                        const void *buffer,
                                        unsigned int to_write,
                                        unsigned int *bytes_written);

/*!
Closes the stream endpoint of the parent process indicated by `stream`.

This function is necessary when a child process reads from stdin until it is
closed. After writing all the input to the child process with `reproc_write`,
the standard input stream can be closed using this function.

Possible errors: /
*/
REPROC_EXPORT void reproc_close(reproc_type *process, REPROC_STREAM stream);

/*!
Waits `timeout` milliseconds for the child process to exit.

If `timeout` is 0 the function will only check if the child process is still
running without waiting. If `timeout` is `REPROC_INFINITE` the function will
wait indefinitely for the child process to exit.

If this function returns `REPROC_SUCCESS`, `process` has exited and its exit
status can be retrieved with `reproc_exit_status`.

Possible errors when `timeout` is 0:
- `REPROC_WAIT_TIMEOUT`

Possible errors when `timeout` is `REPROC_INFINITE`:
- `REPROC_WAIT_TIMEOUT`
- `REPROC_INTERRUPTED`

Possible errors when `timeout` is not 0 or `REPROC_INFINITE`:
- `REPROC_WAIT_TIMEOUT`
- `REPROC_INTERRUPTED`
- `REPROC_PROCESS_LIMIT_REACHED`
- `REPROC_NOT_ENOUGH_MEMORY`
*/
REPROC_EXPORT REPROC_ERROR reproc_wait(reproc_type *process,
                                       unsigned int timeout);

/*! Returns `true` if `process` is still running, `false` otherwise. */
REPROC_EXPORT bool reproc_running(reproc_type *process);

/*!
Sends the `SIGTERM` signal (POSIX) or the `CTRL-BREAK` signal (Windows) to the
child process. Remember that successfull calls to `reproc_wait` and
`reproc_destroy` are required to make sure the child process is completely
cleaned up.
*/
REPROC_EXPORT REPROC_ERROR reproc_terminate(reproc_type *process);

/*!
Sends the `SIGKILL` signal to the child process (POSIX) or calls
`TerminateProcess` (Windows) on the child process. Remember that successfull
calls to `reproc_wait` and `reproc_destroy` are required to make sure the child
process is completely cleaned up.
*/
REPROC_EXPORT REPROC_ERROR reproc_kill(reproc_type *process);

/*! Used to tell `reproc_stop` how to stop a child process. */
typedef enum {
  /*! noop (no operation) */
  REPROC_NOOP = 0,
  /*! `reproc_wait` */
  REPROC_WAIT = 1,
  /*! `reproc_terminate` */
  REPROC_TERMINATE = 2,
  /*! `reproc_kill` */
  REPROC_KILL = 3
} REPROC_CLEANUP;

/*!
Simplifies calling combinations of `reproc_wait`, `reproc_terminate` and
`reproc_kill`. The function executes each specified step and waits (using
`reproc_wait`) until the corresponding timeout expires before continuing with
the next step.

Example:

Wait 10 seconds for the child process to exit on its own before sending
`SIGTERM` (POSIX) or `CTRL-BREAK` (Windows) and waiting five more seconds for
the child process to exit.

```c
REPROC_ERROR error = reproc_stop(process,
                                 REPROC_WAIT, 10000,
                                 REPROC_TERMINATE, 5000,
                                 REPROC_NOOP, 0);
```

Call `reproc_wait`, `reproc_terminate` and `reproc_kill` directly if you need
extra logic such as logging between calls.

(`c1`,`t1`), (`c2`,`t2`) and (`c3`,`t3`) are pairs that instruct `reproc_stop`
how to stop a process. The first element of each pair instructs `reproc_stop` to
execute the corresponding function to stop the process. The second element of
each pair tells `reproc_stop` how long to wait after executing the function
indicated by the first element.

If this function returns `REPROC_SUCCESS`, `process` has exited and its exit
status can be retrieved with `reproc_exit_status`.

See `reproc_wait` for the list of possible errors.
*/
REPROC_EXPORT REPROC_ERROR reproc_stop(reproc_type *process,
                                       REPROC_CLEANUP c1,
                                       unsigned int t1,
                                       REPROC_CLEANUP c2,
                                       unsigned int t2,
                                       REPROC_CLEANUP c3,
                                       unsigned int t3);

/*! Returns the exit status of `process`. It is undefined behaviour to call this
function before `process` has exited. */
REPROC_EXPORT unsigned int reproc_exit_status(reproc_type *process);

/*! Frees all allocated resources stored in `process`. It is undefined behaviour
to call this function before `process` has exited. */
REPROC_EXPORT void reproc_destroy(reproc_type *process);

#ifdef __cplusplus
}
#endif
