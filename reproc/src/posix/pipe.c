// Make sure we get `pipe2` on Linux.
#define _GNU_SOURCE

#include "pipe.h"

#include "error.h"
#include "handle.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

const int PIPE_INVALID = -1;

int pipe_init(int *read, int *write)
{
  assert(read);
  assert(write);

  int pipe[2] = { PIPE_INVALID, PIPE_INVALID };
  int r = -1;

  // We use `socketpair` so we have a POSIX compatible way of setting the pipe
  // size using `setsockopt` with `SO_RECVBUF`.

#if defined(__linux__)
  // `SOCK_CLOEXEC` avoids the race condition between `socketpair` and `fcntl`.
  r = socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, pipe);
  if (r < 0) {
    goto finish;
  }
#else
  r = socketpair(AF_UNIX, SOCK_STREAM, 0, pipe);
  if (r < 0) {
    goto finish;
  }

  r = fcntl(pipe[0], F_SETFD, FD_CLOEXEC);
  if (r < 0) {
    goto finish;
  }

  r = fcntl(pipe[1], F_SETFD, FD_CLOEXEC);
  if (r < 0) {
    goto finish;
  }
#endif

  // `socketpair` gives us a bidirectional socket pair but we only need
  // unidirectional traffic so shut down writes on the read end and vice-versa
  // on the write end.

  // `shutdown` behaves differently on macOS which causes the tests to fail. As
  // this is mostly ceremonial, we disable `shutdown` on macOS until someone
  // with a mac is impacted by this and is willing to further investigate the
  // issue.
#if !defined(__APPLE__)
  r = shutdown(pipe[0], SHUT_WR);
  if (r < 0) {
    goto finish;
  }

  r = shutdown(pipe[1], SHUT_RD);
  if (r < 0) {
    goto finish;
  }
#endif

  *read = pipe[0];
  *write = pipe[1];

  pipe[0] = PIPE_INVALID;
  pipe[1] = PIPE_INVALID;

finish:
  pipe_destroy(pipe[0]);
  pipe_destroy(pipe[1]);

  return error_unify(r);
}

int pipe_read(int pipe, uint8_t *buffer, size_t size)
{
  assert(pipe != PIPE_INVALID);
  assert(buffer);

  int r = (int) read(pipe, buffer, size);
  if (r == 0) {
    // `read` returns 0 to indicate the other end of the pipe was closed.
    r = -EPIPE;
  }

  return error_unify_or_else(r, r);
}

int pipe_write(int pipe, const uint8_t *buffer, size_t size, int timeout)
{
  assert(pipe != PIPE_INVALID);
  assert(buffer);

  struct pollfd pollfd = { .fd = pipe, .events = POLLOUT };
  int r = -1;

  r = poll(&pollfd, 1, timeout);
  if (r <= 0) {
    return error_unify_or_else(r, -ETIMEDOUT);
  }

  r = (int) write(pipe, buffer, size);

  return error_unify_or_else(r, r);
}

int pipe_wait(const int *pipes, size_t num_pipes, int timeout)
{
  assert(pipes);
  assert(num_pipes <= INT_MAX);

  struct pollfd *pollfds = NULL;
  int r = -ENOMEM;

  pollfds = calloc(sizeof(struct pollfd), num_pipes);
  if (pollfds == NULL) {
    goto finish;
  }

  for (size_t i = 0; i < num_pipes; i++) {
    pollfds[i] = (struct pollfd){ .fd = pipes[i], .events = POLLIN };
  }

  r = poll(pollfds, (nfds_t) num_pipes, timeout);
  if (r <= 0) {
    if (r == 0) {
      r = -ETIMEDOUT;
    }

    goto finish;
  }

  size_t i = 0;

  for (; i < num_pipes; i++) {
    struct pollfd pollfd = pollfds[i];

    if (pollfd.revents > 0) {
      break;
    }
  }

  assert(i < num_pipes);
  r = (int) i;

finish:
  free(pollfds);

  return error_unify_or_else(r, r);
}

int pipe_destroy(int pipe)
{
  return handle_destroy(pipe);
}
