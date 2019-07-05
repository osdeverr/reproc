#include <reproc/reproc.h>

#include "fd.h"
#include "pipe.h"
#include "process.h"

#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

// Makeshift C lambda which is passed to `process_create`.
static int exec_process(const void *context)
{
  const char *const *argv = context;

  // Replace the forked process with the process specified in `argv`'s first
  // element. The cast is safe since `execvp` doesn't actually change the
  // contents of `argv`.
  if (execvp(argv[0], (char **) argv) == -1) {
    return errno;
  }

  return 0;
}

REPROC_ERROR reproc_start(reproc_t *process,
                          int argc,
                          const char *const *argv,
                          const char *working_directory)
{
  assert(process);

  assert(argc > 0);
  assert(argv);
  assert(argv[argc] == NULL);

  for (int i = 0; i < argc; i++) {
    assert(argv[i]);
  }

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

  error = pipe_init(&process->err, &child_stderr);
  if (error) {
    goto cleanup;
  }

  struct process_options options = {
    .working_directory = working_directory,
    .stdin_fd = child_stdin,
    .stdout_fd = child_stdout,
    .stderr_fd = child_stderr,
    // We put the child process in its own process group which is needed by
    // `wait_timeout` in `process.c` (see `wait_timeout` for extra information).
    .process_group = 0,
    // Don't return early to make sure we receive errors reported by `execve`.
    .return_early = false,
    // Use vfork to increase performance when forking from a large parent
    // process (`vfork` avoids copying the entire page table).
    .vfork = true
  };

  // Fork a child process and call `execve`.
  error = process_create(exec_process, argv, &options, &process->id);

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
                         REPROC_STREAM stream,
                         uint8_t *buffer,
                         unsigned int size,
                         unsigned int *bytes_read)
{
  assert(process);
  assert(stream != REPROC_STREAM_IN);
  assert(buffer);
  assert(bytes_read);

  switch (stream) {
    case REPROC_STREAM_IN:
      break;
    case REPROC_STREAM_OUT:
      return pipe_read(process->out, buffer, size, bytes_read);
    case REPROC_STREAM_ERR:
      return pipe_read(process->err, buffer, size, bytes_read);
  }

  assert(0);
  return REPROC_ERROR_SYSTEM;
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

  assert(0);
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
