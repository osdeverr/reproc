#pragma once

// Save the current error variable value and system error value and reapply them
// when `UNPROTECT_SYSTEM_ERROR` is called.

#if defined(_WIN32)
  #define PROTECT_SYSTEM_ERROR                                                 \
    BOOL saved_result = r;                                                     \
    DWORD saved_error = r == 0 ? GetLastError() : 0

  #define UNPROTECT_SYSTEM_ERROR                                               \
    do {                                                                       \
      r = saved_result;                                                        \
      SetLastError(saved_error);                                               \
    } while (0)
#else
  #define PROTECT_SYSTEM_ERROR                                                 \
    int saved_result = r;                                                      \
    int saved_error = r == -1 ? errno : 0

  #define UNPROTECT_SYSTEM_ERROR                                               \
    do {                                                                       \
      r = saved_result;                                                        \
      errno = saved_error;                                                     \
    } while (0)
#endif

// Use this to assert inside a `PROTECT/UNPROTECT` block to avoid clang dead
// store warnings.
#define assert_unused(expression)                                              \
  do {                                                                         \
    (void) !(expression);                                                      \
    assert((expression));                                                      \
  } while (0)

// Returns `r` if `expression` is false.
#define assert_return(expression, r)                                           \
  do {                                                                         \
    if (!(expression)) {                                                       \
      return (r);                                                              \
    }                                                                          \
  } while (false)

// Returns a common representation of platform-specific errors. In practice, the
// value of `errno` or `GetLastError` is negated and returned if `r` indicates
// an error occurred (`r < 0` on Linux and `r == 0` on Windows).
//
// Returns 0 if no error occurred. If `r` has already been passed through
// `error_unify` before, it is returned unmodified.
int error_unify(int r);

// `error_unify` but returns `success` if no error occurred.
int error_unify_or_else(int r, int success);

const char *error_string(int error);
