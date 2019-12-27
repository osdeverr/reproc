#include <reproc/reproc.h>

#include "error.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

const int REPROC_ERROR_STREAM_CLOSED = -EPIPE;
const int REPROC_ERROR_WAIT_TIMEOUT = -EAGAIN;

int error_unify(int r, int success)
{
  return r < 0 ? -errno : success;
}

const char *error_string(int error)
{
  return strerror(abs(error));
}
