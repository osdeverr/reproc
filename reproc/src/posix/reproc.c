#include <reproc/reproc.h>

#include "fd.h"
#include "pipe.h"
#include "process.h"

#include <assert.h>
#include <fcntl.h>
#include <poll.h>
#include <stddef.h>

REPROC_ERROR reproc_start(reproc_t *process,
                          const char *const *argv,
                          const char *const *environment,
                          const char *working_directory)
{
  assert(process);
  assert(argv);
  assert(argv[0] != NULL);

  process->running = false;

  // Predeclare every variable so we can use `goto`.

  int child_stdin = 0;
  int child_stdout = 0;
  int child_stderr = 0;

  REPROC_ERROR error = REPROC_SUCCESS;

  // Create the pipes used to redirect the child process' stdin/stdout/stderr to
  // the parent process.

  error = pipe_init(&child_stdin, &process->in);
  if (error) {
    goto cleanup;
  }

  error = pipe_init(&process->out, &child_stdout);
  if (error) {
    goto cleanup;
  }

  // We poll the output pipes so we put the parent ends of the output pipes in
  // non-blocking mode.

  if (fcntl(process->out, F_SETFL, O_NONBLOCK) == -1) {
    return REPROC_ERROR_SYSTEM;
  }

  error = pipe_init(&process->err, &child_stderr);
  if (error) {
    goto cleanup;
  }

  if (fcntl(process->err, F_SETFL, O_NONBLOCK) == -1) {
    return REPROC_ERROR_SYSTEM;
  }

  struct process_options options = { .environment = environment,
                                     .working_directory = working_directory,
                                     .stdin_fd = child_stdin,
                                     .stdout_fd = child_stdout,
                                     .stderr_fd = child_stderr };

  // Fork a child process and call `execve`.
  error = process_create(argv, &options, &process->id);

cleanup:
  // Either an error has ocurred or the child pipe endpoints have been copied to
  // the stdin/stdout/stderr streams of the child process. Either way they can
  // be safely closed in the parent process.
  fd_close(&child_stdin);
  fd_close(&child_stdout);
  fd_close(&child_stderr);

  if (error) {
    reproc_destroy(process);
  } else {
    process->running = true;
  }

  return error;
}

REPROC_ERROR reproc_read(reproc_t *process,
                         REPROC_STREAM *stream,
                         uint8_t *buffer,
                         unsigned int size,
                         unsigned int *bytes_read)
{
  assert(process);
  assert(buffer);
  assert(bytes_read);

  // See 10.0.0 changelog for why we use `poll`.

  struct pollfd fds[2] = { { .fd = process->out, .events = POLLIN },
                           { .fd = process->err, .events = POLLIN } };
  // -1 tells `poll` we want to wait indefinitely for events.
  if (poll(&fds[0], 2, -1) == -1) {
    return REPROC_ERROR_SYSTEM;
  }

  for (size_t i = 0; i < 2; i++) {
    struct pollfd pollfd = fds[i];

    // Indicate which stream was read from if requested by the user.
    if (stream != NULL) {
      *stream = pollfd.fd == process->out ? REPROC_STREAM_OUT
                                          : REPROC_STREAM_ERR;
    }

    if (pollfd.revents & POLLIN || pollfd.revents & POLLERR) {
      return pipe_read(pollfd.fd, buffer, size, bytes_read);
    }
  }

  // Both streams should have been closed by the child process if we get here.
  assert(fds[0].revents & POLLHUP && fds[1].revents & POLLHUP);

  return REPROC_ERROR_STREAM_CLOSED;
}

REPROC_ERROR reproc_write(reproc_t *process,
                          const uint8_t *buffer,
                          unsigned int size,
                          unsigned int *bytes_written)
{
  assert(process);
  assert(process->in != 0);
  assert(buffer);
  assert(bytes_written);

  return pipe_write(process->in, buffer, size, bytes_written);
}

void reproc_close(reproc_t *process, REPROC_STREAM stream)
{
  assert(process);

  switch (stream) {
    case REPROC_STREAM_IN:
      fd_close(&process->in);
      return;
    case REPROC_STREAM_OUT:
      fd_close(&process->out);
      return;
    case REPROC_STREAM_ERR:
      fd_close(&process->err);
      return;
  }

  assert(false);
}

REPROC_ERROR reproc_wait(reproc_t *process, unsigned int timeout)
{
  assert(process);

  if (!process->running) {
    return REPROC_SUCCESS;
  }

  REPROC_ERROR error = process_wait(process->id, timeout,
                                    &process->exit_status);

  if (error == REPROC_SUCCESS) {
    process->running = false;
  }

  return error;
}

REPROC_ERROR reproc_terminate(reproc_t *process)
{
  assert(process);

  if (!process->running) {
    return REPROC_SUCCESS;
  }

  return process_terminate(process->id);
}

REPROC_ERROR reproc_kill(reproc_t *process)
{
  assert(process);

  if (!process->running) {
    return REPROC_SUCCESS;
  }

  return process_kill(process->id);
}

void reproc_destroy(reproc_t *process)
{
  assert(process);

  fd_close(&process->in);
  fd_close(&process->out);
  fd_close(&process->err);
}
