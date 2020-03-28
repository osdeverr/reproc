#define _WIN32_WINNT _WIN32_WINNT_VISTA

#include "redirect.h"

#include "error.h"
#include "pipe.h"
#include "utf.h"

#include <io.h>
#include <stdlib.h>
#include <windows.h>

static DWORD stream_to_id(REPROC_STREAM stream)
{
  switch (stream) {
    case REPROC_STREAM_IN:
      return STD_INPUT_HANDLE;
    case REPROC_STREAM_OUT:
      return STD_OUTPUT_HANDLE;
    case REPROC_STREAM_ERR:
      return STD_ERROR_HANDLE;
  }

  return 0;
}

int redirect_parent(HANDLE *child, REPROC_STREAM stream)
{
  ASSERT(child);
  int r = 0;

  DWORD id = stream_to_id(stream);
  if (id == 0) {
    return -ERROR_INVALID_PARAMETER;
  }

  HANDLE *handle = GetStdHandle(id);
  if (handle == INVALID_HANDLE_VALUE) {
    return error_unify(r);
  }

  if (handle == NULL) {
    return -ERROR_BROKEN_PIPE;
  }

  *child = handle;

  return 0;
}

enum { FILE_NO_TEMPLATE = 0 };

static SECURITY_ATTRIBUTES INHERIT = { .nLength = sizeof(SECURITY_ATTRIBUTES),
                                       .bInheritHandle = true,
                                       .lpSecurityDescriptor = NULL };

int redirect_discard(HANDLE *child, REPROC_STREAM stream)
{
  return redirect_path(child, stream, "NUL");
}

int redirect_file(HANDLE *child, FILE *file)
{
  int r = -1;

  r = _fileno(file);
  if (r < 0) {
    return -ERROR_INVALID_HANDLE;
  }

  intptr_t result = _get_osfhandle(r);
  if (result == -1) {
    return -ERROR_INVALID_HANDLE;
  }

  *child = (HANDLE) result;

  return error_unify(r);
}

int redirect_path(handle_type *child,
                  REPROC_STREAM stream,
                  const char *path)
{
  ASSERT(child);
  ASSERT(path);

  DWORD mode = stream == REPROC_STREAM_IN ? GENERIC_READ : GENERIC_WRITE;
  HANDLE handle = HANDLE_INVALID;
  int r = 0;

  wchar_t *wpath = utf16_from_utf8(path, strlen(path));
  if (wpath == NULL) {
    goto finish;
  }

  handle = CreateFileW(wpath, mode, FILE_SHARE_READ | FILE_SHARE_WRITE,
                       &INHERIT, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL,
                       (HANDLE) FILE_NO_TEMPLATE);
  if (handle == INVALID_HANDLE_VALUE) {
    goto finish;
  }

  *child = handle;
  handle = HANDLE_INVALID;
  r = 1;

finish:
  free(wpath);
  handle_destroy(handle);

  return error_unify(r);
}
