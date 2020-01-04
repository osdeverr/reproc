#pragma once

#include <reproc/export.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*! Used to store information about a child process. `reproc_t` is an opaque
type and can be allocated and released via `reproc_new` and `reproc_destroy`
respectively. */
typedef struct reproc_t reproc_t;

/*! reproc error naming follows POSIX errno naming prefixed with `REPROC`. */

/*! An invalid argument was passed to an API function */
REPROC_EXPORT extern const int REPROC_EINVAL;
/*! A timeout value passed to an API function expired. */
REPROC_EXPORT extern const int REPROC_ETIMEDOUT;
/*! The child process closed one of its streams (and in the case of
stdout/stderr all of the data remaining in that stream has been read). */
REPROC_EXPORT extern const int REPROC_EPIPE;
/*! A memory allocation failed. */
REPROC_EXPORT extern const int REPROC_ENOMEM;

/*! Signal exit status constants. */

REPROC_EXPORT extern const int REPROC_SIGKILL;
REPROC_EXPORT extern const int REPROC_SIGTERM;

/*! Tells a function that takes a timeout value to wait indefinitely. */
REPROC_EXPORT extern const int REPROC_INFINITE;

/*! Stream identifiers used to indicate which stream to act on. */
typedef enum {
  /*! stdin */
  REPROC_STREAM_IN,
  /*! stdout */
  REPROC_STREAM_OUT,
  /*! stderr */
  REPROC_STREAM_ERR
} REPROC_STREAM;

/*! Used to tell reproc where to redirect the streams of the child process. */
typedef enum {
  /*! Redirect the stream to a pipe. */
  REPROC_REDIRECT_PIPE,
  /*! Inherit the corresponding stream from the parent process. */
  REPROC_REDIRECT_INHERIT,
  /*! Redirect the stream to /dev/null (or NUL on Windows). */
  REPROC_REDIRECT_DISCARD
} REPROC_REDIRECT;

/*! Used to tell `reproc_stop` how to stop a child process. */
typedef enum {
  /*! noop (no operation) */
  REPROC_STOP_NOOP,
  /*! `reproc_wait` */
  REPROC_STOP_WAIT,
  /*! `reproc_terminate` */
  REPROC_STOP_TERMINATE,
  /*! `reproc_kill` */
  REPROC_STOP_KILL,
} REPROC_STOP;

typedef struct reproc_stop_action {
  REPROC_STOP action;
  int timeout;
} reproc_stop_action;

typedef struct reproc_stop_actions {
  reproc_stop_action first;
  reproc_stop_action second;
  reproc_stop_action third;
} reproc_stop_actions;

typedef struct reproc_options {
  /*!
  `environment` is an array of UTF-8 encoded, null terminated strings that
  specifies the environment for the child process. It has the following layout:

  - All elements except the final element must be of the format `NAME=VALUE`.
  - The final element must be `NULL`.

  Example: ["IP=127.0.0.1", "PORT=8080", `NULL`]

  If `environment` is `NULL`, the child process inherits the environment of the
  current process.
  */
  const char *const *environment;
  /*!
  `working_directory` specifies the working directory for the child process. If
  `working_directory` is `NULL`, the child process runs in the working directory
  of the parent process.
  */
  const char *working_directory;
  /*!
  `redirect` specifies where to redirect the streams from the child process.

  By default each stream is redirected to a pipe which can be written to (stdin)
  or read from (stdout/stderr) using `reproc_write` and `reproc_read`
  respectively.
  */
  struct {
    REPROC_REDIRECT in;
    REPROC_REDIRECT out;
    REPROC_REDIRECT err;
  } redirect;
  /*!
  Stop actions that are passed to `reproc_stop` in `reproc_destroy` to stop the
  child process.

  When `stop_actions` is 3x `REPROC_STOP_NOOP`, `reproc_destroy` will default to
  waiting indefinitely for the child process to exit.
  */
  reproc_stop_actions stop_actions;
} reproc_options;

/*! Used by `reproc_drain` to provide data to the caller. Each time data is
read, `function` is called with `context`. See `reproc_drain` and the `drain`
example for more information .*/
typedef struct reproc_sink {
  bool (*function)(REPROC_STREAM stream,
                   const uint8_t *buffer,
                   size_t size,
                   void *context);
  void *context;
} reproc_sink;

/*! Allocate a new `reproc_t` instance on the heap. */
REPROC_EXPORT reproc_t *reproc_new(void);

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
*/
REPROC_EXPORT int reproc_start(reproc_t *process,
                               const char *const *argv,
                               reproc_options options);

/*!
Reads up to `size` bytes into `buffer` from either the child process stdout or
stderr stream and returns the amount of bytes read.

If `stream` is not `NULL`, it is used to store the stream that was
read from (`REPROC_STREAM_OUT` or `REPROC_STREAM_ERR`).

If no output stream is closed or read from within the given timeout, this
function returns `REPROC_ETIMEDOUT`. If one of the output streams is closed,
`timeout` is reset before waiting again for the other stream.

If both streams are closed by the child process or weren't opened with
`REPROC_REDIRECT_PIPE`, this function returns `REPROC_EPIPE`.

Actionable errors:
- `REPROC_EPIPE`
- `REPROC_ETIMEDOUT`
*/
REPROC_EXPORT int reproc_read(reproc_t *process,
                              REPROC_STREAM *stream,
                              uint8_t *buffer,
                              size_t size,
                              int timeout);

/*!
Calls `reproc_read` on `stream` until `reproc_read` returns an error or one of
the sinks returns false. The `out` and `err` sinks receive the output from
stdout and stderr respectively. The same sink may be passed to both `out` and
`err`.

If `out` or `err` are `NULL`, all output on the corresponding stream is
discarded.

`reproc_drain` always starts by calling both sinks once with an empty buffer and
`stream` set to `REPROC_STREAM_IN` to give each sink the chance to process all
output from the previous call to `reproc_drain` one by one.

Each call to `reproc_read` is passed the given timeout. If a call to
`reproc_read` times out, this function returns `REPROC_ETIMEDOUT`.

Note that his function returns 0 instead of `REPROC_EPIPE` when both output
streams of the child process are closed.

For examples of sinks, see `sink.h`.

Actionable errors:
- `REPROC_ETIMEDOUT`
*/
REPROC_EXPORT int reproc_drain(reproc_t *process,
                               reproc_sink *out,
                               reproc_sink *err,
                               int timeout);

/*!
Writes `size` bytes from `buffer` to the standard input (stdin) of the child
process.

If no data can be written within the given timeout, this function returns
`REPROC_ETIMEDOUT`. After writing some data, `timeout` is reset before trying to
write again.

(POSIX) By default, writing to a closed stdin pipe terminates the parent process
with the `SIGPIPE` signal. `reproc_write` will only return `REPROC_EPIPE` if
this signal is ignored by the parent process.

This function can only be used if the standard input of the child process was
redirected to a pipe by using `REPROC_REDIRECT_PIPE` for the standard input in
the redirect options passed to `reproc_start`.

Actionable errors:
- `REPROC_EPIPE`
- `REPROC_ETIMEDOUT`
*/
REPROC_EXPORT int reproc_write(reproc_t *process,
                               const uint8_t *buffer,
                               size_t size,
                               int timeout);

/*!
Closes the child process standard stream indicated by `stream`.

This function is necessary when a child process reads from stdin until it is
closed. After writing all the input to the child process using `reproc_write`,
the standard input stream can be closed using this function.
*/
REPROC_EXPORT int reproc_close(reproc_t *process, REPROC_STREAM stream);

/*!
Waits `timeout` milliseconds for the child process to exit. If the child process
has already exited or exits within the given timeout, its exit status is
returned.

If `timeout` is 0, the function will only check if the child process is still
running without waiting. If `timeout` is `REPROC_INFINITE`, the function will
wait indefinitely for the child process to exit.

Actionable errors:
- `REPROC_ETIMEDOUT`
*/
REPROC_EXPORT int reproc_wait(reproc_t *process, int timeout);

/*!
Sends the `SIGTERM` signal (POSIX) or the `CTRL-BREAK` signal (Windows) to the
child process. Remember that successful calls to `reproc_wait` and
`reproc_destroy` are required to make sure the child process is completely
cleaned up.
*/
REPROC_EXPORT int reproc_terminate(reproc_t *process);

/*!
Sends the `SIGKILL` signal to the child process (POSIX) or calls
`TerminateProcess` (Windows) on the child process. Remember that successful
calls to `reproc_wait` and `reproc_destroy` are required to make sure the child
process is completely cleaned up.
*/
REPROC_EXPORT int reproc_kill(reproc_t *process);

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
                                 REPROC_STOP_WAIT, 10000,
                                 REPROC_STOP_TERMINATE, 5000,
                                 REPROC_STOP_NOOP, 0);
```

Call `reproc_wait`, `reproc_terminate` and `reproc_kill` directly if you need
extra logic such as logging between calls.

`stop_actions` can contain up to three stop actions that instruct this function
how the child process should be stopped. The first element of each stop action
specifies which action should be called on the child process. The second element
of each stop actions specifies how long to wait after executing the operation
indicated by the first element.

Note that when a stop action specifies `REPROC_STOP_WAIT`, the function will
just wait for the specified timeout instead of performing an action to stop the
child process.

If the child process has already exited or exits during the execution of this
function, its exit status is returned.

Actionable errors:
- `REPROC_ETIMEDOUT`
*/
REPROC_EXPORT int reproc_stop(reproc_t *process,
                              reproc_stop_actions stop_actions);

/*!
Release all resources associated with `process` including the memory allocated
by `reproc_new`. Calling this function before a succesfull call to `reproc_wait`
can result in resource leaks.

Does not nothing if `process` is an invalid `reproc_t` instance and always
returns an invalid `reproc_t` instance (`NULL`). By assigning the result of
`reproc_destroy` to the instance being destroyed, it can be safely called
multiple times on the same instance.

Example: `process = reproc_destroy(process)`.
*/
REPROC_EXPORT reproc_t *reproc_destroy(reproc_t *process);

/*!
Returns a string describing `error`. This string must not be modified by the
caller.
*/
REPROC_EXPORT const char *reproc_strerror(int error);

#ifdef __cplusplus
}
#endif
