#pragma once

#include <reproc/error.h>

#include <stdbool.h>
#include <stdint.h>

typedef void *HANDLE;

// Creates a new anonymous pipe. `read` and `write` are set to the read and
// write handle of the pipe respectively. `inherit_read` and `inherit_write`
// specify whether the `read` or `write` handle respectively should be inherited
// by any child processes spawned by the current process.
REPROC_ERROR
pipe_init(HANDLE *read, bool inherit_read, HANDLE *write, bool inherit_write);

// Reads up to `size` bytes from the pipe indicated by `handle` and stores them
// them in `buffer`. The amount of bytes read is stored in `bytes_read`.
REPROC_ERROR pipe_read(HANDLE pipe,
                       uint8_t *buffer,
                       unsigned int size,
                       unsigned int *bytes_read);

// Writes up to `size` bytes from `buffer` to the pipe indicated by `handle`
// and stores the amount of bytes written in `bytes_written`.
//
// For pipes, the `write` system call on Windows platforms will block until the
// requested amount of bytes have been written to the pipe so this function
// should only rarely succeed without writing the full amount of bytes
// requested.
REPROC_ERROR pipe_write(HANDLE pipe,
                        const uint8_t *buffer,
                        unsigned int size,
                        unsigned int *bytes_written);
