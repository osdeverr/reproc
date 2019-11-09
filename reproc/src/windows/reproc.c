#include <reproc/reproc.h>

#include <windows/handle.h>
#include <windows/pipe.h>
#include <windows/process.h>
#include <windows/redirect.h>

#include <assert.h>
#include <stdlib.h>
#include <string.h>

REPROC_ERROR
reproc_start(reproc_t *process, const char *const *argv, reproc_options options)
{
  assert(process);
  assert(argv);
  assert(argv[0] != NULL);

  HANDLE child_in = NULL;
  HANDLE child_out = NULL;
  HANDLE child_err = NULL;

  REPROC_ERROR error = REPROC_ERROR_SYSTEM;

  *process = (reproc_t){ 0 };

  error = redirect(&process->in, &child_in, REPROC_STREAM_IN,
                   options.redirect.in);
  if (error) {
    goto cleanup;
  }

  error = redirect(&process->out, &child_out, REPROC_STREAM_OUT,
                   options.redirect.out);
  if (error) {
    goto cleanup;
  }

  error = redirect(&process->err, &child_err, REPROC_STREAM_ERR,
                   options.redirect.err);
  if (error) {
    goto cleanup;
  }

  struct process_options process_options = {
    .environment = options.environment,
    .working_directory = options.working_directory,
    .redirect = { .in = child_in, .out = child_out, .err = child_err }
  };

  error = process_create(&process->handle, argv, process_options);
  if (error) {
    goto cleanup;
  }

  error = REPROC_SUCCESS;

cleanup:
  // Either an error has ocurred or the child pipe endpoints have been copied to
  // the stdin/stdout/stderr streams of the child process. Either way, they can
  // be safely closed in the parent process.
  handle_close(&child_in);
  handle_close(&child_out);
  handle_close(&child_err);

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

  HANDLE ready = NULL;

  REPROC_ERROR error = pipe_wait(&ready, process->out, process->err);
  if (error) {
    return error;
  }

  error = pipe_read(ready, buffer, size, bytes_read);
  if (error) {
    return error;
  }

  if (stream) {
    *stream = ready == process->out ? REPROC_STREAM_OUT : REPROC_STREAM_ERR;
  }

  return REPROC_SUCCESS;
}

REPROC_ERROR
reproc_write(reproc_t *process, const uint8_t *buffer, unsigned int size)
{
  assert(process);
  assert(process->in);
  assert(buffer);

  do {
    unsigned int bytes_written = 0;

    REPROC_ERROR error = pipe_write(process->in, buffer, size, &bytes_written);
    if (error) {
      return error;
    }

    assert(bytes_written <= size);
    buffer += bytes_written;
    size -= bytes_written;
  } while (size != 0);

  return REPROC_SUCCESS;
}

void reproc_close(reproc_t *process, REPROC_STREAM stream)
{
  assert(process);

  switch (stream) {
    case REPROC_STREAM_IN:
      handle_close(&process->in);
      return;
    case REPROC_STREAM_OUT:
      handle_close(&process->out);
      return;
    case REPROC_STREAM_ERR:
      handle_close(&process->err);
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

  REPROC_ERROR error = process_wait(process->handle, timeout,
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

  return process_terminate(process->handle);
}

REPROC_ERROR reproc_kill(reproc_t *process)
{
  assert(process);

  if (!process->running) {
    return REPROC_SUCCESS;
  }

  return process_kill(process->handle);
}

void reproc_destroy(reproc_t *process)
{
  assert(process);

  handle_close(&process->handle);

  handle_close(&process->in);
  handle_close(&process->out);
  handle_close(&process->err);
}
