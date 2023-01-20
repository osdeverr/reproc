#ifdef RE_REPROC_BUILD_POSIX_IMPL

#define _POSIX_C_SOURCE 200809L

#include "init.h"

int init(void)
{
  return 0;
}

void deinit(void) {}

#endif // RE_REPROC_BUILD_*_IMPL
