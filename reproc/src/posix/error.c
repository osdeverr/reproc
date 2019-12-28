#include <reproc/reproc.h>

#include "error.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

const int REPROC_EINVAL = -EINVAL;
const int REPROC_EPIPE = -EPIPE;
const int REPROC_ETIMEDOUT = -ETIMEDOUT;
const int REPROC_EINPROGRESS = -EINPROGRESS;
const int REPROC_ENOMEM = -ENOMEM;

int error_unify(int r, int success)
{
  return r < 0 ? -errno : success;
}

const char *error_string(int error)
{
  return strerror(abs(error));
}
