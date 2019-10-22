#include <reproc/reproc.h>

#include <windows/handle.h>
#include <windows/pipe.h>
#include <windows/process.h>

#include <assert.h>
#include <stdlib.h>
#include <string.h>

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

  HANDLE child_stdin = NULL;
  HANDLE child_stdout = NULL;
  HANDLE child_stderr = NULL;

  char *command_line = NULL;
  wchar_t *command_line_wstring = NULL;
  char *environment_line = NULL;
  wchar_t *environment_line_wstring = NULL;
  wchar_t *working_directory_wstring = NULL;

  // Initialize to `REPROC_ERROR_SYSTEM` so we don't accidentally return
  // `REPROC_SUCCESS`.
  REPROC_ERROR error = REPROC_ERROR_SYSTEM;

  const struct pipe_options child_blocking = { .inherit = true,
                                               .overlapped = false };
  const struct pipe_options parent_overlapped = { .inherit = false,
                                                  .overlapped = true };

  // While we already make sure the child process only inherits the child pipe
  // handles using `STARTUPINFOEXW` (see `process.c`) we still disable
  // inheritance of the parent pipe handles to lower the chance of child
  // processes not created by reproc unintentionally inheriting these handles.

  error = pipe_init(&child_stdin, child_blocking, &process->in,
                    parent_overlapped);
  if (error) {
    goto cleanup;
  }

  error = pipe_init(&process->out, parent_overlapped, &child_stdout,
                    child_blocking);
  if (error) {
    goto cleanup;
  }

  error = pipe_init(&process->err, parent_overlapped, &child_stderr,
                    child_blocking);
  if (error) {
    goto cleanup;
  }

  // Join `argv` to a whitespace delimited string as required by
  // `CreateProcessW`.
  command_line = argv_join(argv);
  if (command_line == NULL) {
    goto cleanup;
  }

  // Convert UTF-8 to UTF-16 as required by `CreateProcessW`.
  command_line_wstring = string_to_wstring(command_line,
                                           strlen(command_line) + 1);
  if (command_line_wstring == NULL) {
    goto cleanup;
  }

  // Idem for `environment` if it isn't `NULL`.
  if (environment != NULL) {
    environment_line = environment_join(environment);
    if (environment_line == NULL) {
      goto cleanup;
    }

    environment_line_wstring =
        string_to_wstring(environment_line, environment_join_size(environment));
    if (environment_line_wstring == NULL) {
      goto cleanup;
    }
  }

  // Convert `working_directory` to UTF-16 if it isn't `NULL`.
  if (working_directory != NULL) {
    size_t working_directory_size = strlen(working_directory) + 1;
    working_directory_wstring = string_to_wstring(working_directory,
                                                  working_directory_size);
    if (working_directory_wstring == NULL) {
      goto cleanup;
    }
  }

  struct process_options options = {
    .environment = environment_line_wstring,
    .working_directory = working_directory_wstring,
    .stdin_handle = child_stdin,
    .stdout_handle = child_stdout,
    .stderr_handle = child_stderr
  };

  error = process_create(command_line_wstring, &options, &process->id,
                         &process->handle);
  if (error) {
    goto cleanup;
  }

  error = REPROC_SUCCESS;

cleanup:
  // Either an error has ocurred or the child pipe endpoints have been copied to
  // the stdin/stdout/stderr streams of the child process. Either way they can
  // be safely closed in the parent process.
  handle_close(&child_stdin);
  handle_close(&child_stdout);
  handle_close(&child_stderr);

  free(command_line);
  free(command_line_wstring);
  free(environment_line);
  free(environment_line_wstring);
  free(working_directory_wstring);

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

REPROC_ERROR reproc_write(reproc_t *process,
                          const uint8_t *buffer,
                          unsigned int size,
                          unsigned int *bytes_written)
{
  assert(process);
  assert(process->in);
  assert(buffer);
  assert(bytes_written);

  return pipe_write(process->in, buffer, size, bytes_written);
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

  return process_terminate(process->id);
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
