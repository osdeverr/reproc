#include "pipe.h"
#include "handle.h"

#include <assert.h>

// Ensures both endpoints of the pipe are inherited by the child process.
static SECURITY_ATTRIBUTES security_attributes = {
  .nLength = sizeof(SECURITY_ATTRIBUTES),
  .bInheritHandle = TRUE,
  .lpSecurityDescriptor = NULL
};

// Disables a single endpoint of a pipe from being inherited by the child
// process.
static REPROC_ERROR pipe_disable_inherit(HANDLE pipe)
{
  assert(pipe);

  if (!SetHandleInformation(pipe, HANDLE_FLAG_INHERIT, 0)) {
    return REPROC_ERROR_SYSTEM;
  }

  return REPROC_SUCCESS;
}

REPROC_ERROR
pipe_init(HANDLE *read, bool inherit_read, HANDLE *write, bool inherit_write)
{
  assert(read);
  assert(write);

  if (!CreatePipe(read, write, &security_attributes, 0)) {
    return REPROC_ERROR_SYSTEM;
  }

  REPROC_ERROR error = REPROC_SUCCESS;

  if (!inherit_read) {
    error = pipe_disable_inherit(*read);
    if (error) {
      goto cleanup;
    }
  }

  if (!inherit_write) {
    error = pipe_disable_inherit(*write);
    if (error) {
      goto cleanup;
    }
  }

cleanup:
  if (error) {
    handle_close(read);
    handle_close(write);
  }

  return error;
}

REPROC_ERROR pipe_read(HANDLE pipe,
                       uint8_t *buffer,
                       unsigned int size,
                       unsigned int *bytes_read)
{
  assert(pipe);
  assert(buffer);
  assert(bytes_read);

  // The cast is safe since `DWORD` is a typedef to `unsigned int` on Windows.
  if (!ReadFile(pipe, buffer, size, (LPDWORD) bytes_read, NULL)) {
    switch (GetLastError()) {
      case ERROR_BROKEN_PIPE:
        return REPROC_ERROR_STREAM_CLOSED;
      default:
        return REPROC_ERROR_SYSTEM;
    }
  }

  return REPROC_SUCCESS;
}

REPROC_ERROR pipe_write(HANDLE pipe,
                        const uint8_t *buffer,
                        unsigned int size,
                        unsigned int *bytes_written)
{
  assert(pipe);
  assert(buffer);
  assert(bytes_written);

  // The cast is safe since `DWORD` is a typedef to `unsigned int` on Windows.
  if (!WriteFile(pipe, buffer, size, (LPDWORD) bytes_written, NULL)) {
    switch (GetLastError()) {
      case ERROR_BROKEN_PIPE:
        return REPROC_ERROR_STREAM_CLOSED;
      default:
        return REPROC_ERROR_SYSTEM;
    }
  }

  if (*bytes_written != size) {
    return REPROC_ERROR_PARTIAL_WRITE;
  }

  return REPROC_SUCCESS;
}
