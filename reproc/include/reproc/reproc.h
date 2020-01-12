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
/*! Tells `reproc_wait` to wait until the deadline passed to `reproc_start`
expires. */
REPROC_EXPORT extern const int REPROC_DEADLINE;

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

  If `inherit` or `discard` are set, this option may not be set.
  */
  struct {
    REPROC_REDIRECT in;
    REPROC_REDIRECT out;
    REPROC_REDIRECT err;
  } redirect;
  /*!
  Stop actions that are passed to `reproc_stop` in `reproc_destroy` to stop the
  child process.

  When `stop` is 3x `REPROC_STOP_NOOP`, `reproc_destroy` will default to
  waiting indefinitely for the child process to exit.
  */
  reproc_stop_actions stop;
  /*!
  Maximum duration in milliseconds to wait for a single `reproc_read` or
  `reproc_write` operation to complete. If the timeout expires, the call to
  `reproc_read` or `reproc_write` returns `REPROC_ETIMEDOUT`.

  When `timeout` is zero, `reproc_read` and `reproc_write` will wait
  indefinitely for any I/O to complete.
  */
  int timeout;
  /*!
  Maximum allowed duration in milliseconds the process is allowed to run in
  milliseconds. If the deadline is exceeded, Any ongoing and future calls to
  `reproc_read` and `reproc_write` return `REPROC_ETIMEDOUT`.

  When `deadline` is zero, no deadline is set for the process.
  */
  int deadline;
  /*!
  Shorthand for setting all members of `redirect` to `REPROC_REDIRECT_INHERIT`.
  If `discard` or `redirect` are set, this option may not be set.
  */
  bool inherit;
  /*!
  Shorthand for setting all members of `redirect` to `REPROC_REDIRECT_DISCARD`.
  If `inherit` or `redirect` are set, this option may not be set.
  */
  bool discard;
  /*!
  `input` is written to the stdin pipe before the child process is started.

  Because `input` is written to the stdin pipe before the process starts,
  `input.size` must be smaller than the system's default pipe size (64KB).

  If `input` is set, the stdin pipe is closed after `input` is written to it.

  If `redirect.in` is set, this option may not be set.
  */
  struct {
    const uint8_t *data;
    size_t size;
  } input;
  /*!
  This option can only be used on POSIX systems. If enabled on Windows, an error
  will be returned.

  If `fork` is enabled, `reproc_start` forks a child process and returns 0 in
  the child process and > 0 in the parent process. In the child process, only
  `reproc_destroy` may be called on the `reproc_t` instance to free its
  associated memory.

  When `fork` is enabled. `argv` must be `NULL` when calling `reproc_start`.
  */
  bool fork;
} reproc_options;

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

If both streams are closed by the child process or weren't opened with
`REPROC_REDIRECT_PIPE`, this function returns `REPROC_EPIPE`.

Actionable errors:
- `REPROC_EPIPE`
- `REPROC_ETIMEDOUT`
*/
REPROC_EXPORT int reproc_read(reproc_t *process,
                              REPROC_STREAM *stream,
                              uint8_t *buffer,
                              size_t size);

/*!
Writes `size` bytes from `buffer` to the standard input (stdin) of the child
process.

(POSIX) By default, writing to a closed stdin pipe terminates the parent process
with the `SIGPIPE` signal. `reproc_write` will only return `REPROC_EPIPE` if
this signal is ignored by the parent process.

if `buffer` is `NULL` and `size` is zero, this function returns 0.

If the standard input of the child process wasn't opened with
`REPROC_REDIRECT_PIPE`, this function returns `REPROC_EPIPE` unless `buffer` is
`NULL` and `size` is zero.

Actionable errors:
- `REPROC_EPIPE`
- `REPROC_ETIMEDOUT`
*/
REPROC_EXPORT int
reproc_write(reproc_t *process, const uint8_t *buffer, size_t size);

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
REPROC_EXPORT int reproc_stop(reproc_t *process, reproc_stop_actions stop);

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
