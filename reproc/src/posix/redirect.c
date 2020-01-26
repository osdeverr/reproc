#include "redirect.h"

#include "error.h"
#include "pipe.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>

static FILE *stream_to_file(REDIRECT_STREAM stream)
{
  switch (stream) {
    case REDIRECT_STREAM_IN:
      return stdin;
    case REDIRECT_STREAM_OUT:
      return stdout;
    case REDIRECT_STREAM_ERR:
      return stderr;
  }

  return NULL;
}

int redirect_inherit(int *handle, REDIRECT_STREAM stream)
{
  assert(handle);

  int r = -EINVAL;

  FILE *file = stream_to_file(stream);
  if (file == NULL) {
    return r;
  }

  r = fileno(file);
  if (r < 0) {
    if (errno == EBADF) {
      errno = EPIPE;
    }

    return error_unify(r);
  }

  r = fcntl(r, F_DUPFD_CLOEXEC, 0);
  if (r < 0) {
    return error_unify(r);
  }

  *handle = r; // `r` contains the duplicated file descriptor.

  return 0;
}

int redirect_discard(int *handle, REDIRECT_STREAM stream)
{
  assert(handle);

  int mode = stream == REDIRECT_STREAM_IN ? O_RDONLY : O_WRONLY;

  int r = open("/dev/null", mode | O_CLOEXEC);
  if (r < 0) {
    return error_unify(r);
  }

  *handle = r;

  return 0;
}
