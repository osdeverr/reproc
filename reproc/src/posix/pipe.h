#ifndef REPROC_POSIX_PIPE_H
#define REPROC_POSIX_PIPE_H

#include <reproc/error.h>

// Creates a new anonymous pipe. `read` and `write` are set to the read and
// write end of the pipe respectively.
REPROC_ERROR pipe_init(int *read, int *write);

// Reads up to `size` bytes from the pipe indicated by `pipe` and stores them
// them in `buffer`. The amount of bytes read is stored in `bytes_read`.
//
// Possible errors:
// - `REPROC_STREAM_CLOSED`
// - `REPROC_INTERRUPTED`
REPROC_ERROR pipe_read(int pipe, void *buffer, unsigned int size,
                       unsigned int *bytes_read);

// Writes up to `to_write` bytes from `buffer` to the pipe indicated by `pipe`
// and stores the amount of bytes written in `bytes_written`.
//
// For pipes, the `write` system call on POSIX platforms will block until the
// requested amount of bytes have been written to the pipe so this function
// should only rarely succeed without writing the full amount of bytes
// requested.
//
// By default, writing to a closed pipe terminates a process with the `SIGPIPE`
// signal. `pipe_write` will only return `REPROC_STREAM_CLOSED` if this signal
// is ignored by the running process.
//
// Possible errors:
// - `REPROC_STREAM_CLOSED`
// - `REPROC_INTERRUPTED`
// - `REPROC_PARTIAL_WRITE`
REPROC_ERROR pipe_write(int pipe, const void *buffer, unsigned int to_write,
                        unsigned int *bytes_written);

#endif
