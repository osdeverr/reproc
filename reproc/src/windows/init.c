#include "init.h"

#include "error.h"

#include <assert.h>
#include <winsock2.h>

int init(void)
{
  WSADATA data;
  int r = WSAStartup(MAKEWORD(2, 2), &data);
  return error_unify(r);
}

void deinit(void)
{
  int saved = WSAGetLastError();

  int r = WSACleanup();
  assert_unused(r == 0);

  WSASetLastError(saved);
}
