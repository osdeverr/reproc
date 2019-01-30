# Changelog

## 4.0.0

### General

- Internal improvements and documentation fixes.

### reproc

- Added `reproc_parse` which mimics reproc++'s `process::parse`.
- Added `reproc_drain` which mimics reproc++'s `process::drain` along with an
  example that explains how to use it.

  Because C doesn't support lambda's, both of these functions take a function
  pointer and an extra context argument which is passed to the function pointer
  each time it is called. The context argument can be used to store any data
  needed by the given function pointer.

### reproc++

- Renamed the `process::read` overload which takes a parser to `process::parse`.

  This breaking change was done to keep consistency with reproc where we added
  `reproc_parse`. We couldn't add another `reproc_read` since C doesn't support
  overloading so we made the decision to rename `process::read` to
  `process::parse` instead.

- Changed `process::drain` sinks to return a boolean instead of `void`.

  Before this change, the only way to stop draining a process was to throw an
  exception from the sink. By changing sinks to return `bool`, a sink can tell
  `drain` to stop if an error occurs by returning `false`. The error itself can
  be stored in the sink if needed.

## 3.1.3

### CMake

- Update project version in CMakeLists.txt from 3.0.0 to the actual latest
  version (3.1.3).

## 3.1.2

### pkg-config

- Fix pkg-config install prefix.

## 3.1.0

### CMake

- Added `REPROC_INSTALL_PKGCONFIG` to control whether pkg-config files are
  installed or not (default: `ON`).

  The vcpkg package manager has no need for the pkg-config files so we added an
  option to disable installing them.

- Added `REPROC_INSTALL_CMAKECONFIGDIR` and `REPROC_INSTALL_PKGCONFIGDIR` to
  control where cmake config files and pkg-config files are installed
  respectively (default: `${CMAKE_INSTALL_LIBDIR}/cmake` and
  `${CMAKE_INSTALL_LIBDIR}/pkgconfig`).

  reproc already uses the values from `GNUInstallDirs` when generating its
  install rules which are cache variables that be overridden by users. However,
  `GNUInstallDirs` does not include variables for the installation directories
  of CMake config files and pkg-config files. vcpkg requires cmake config files
  to be installed to a different directory than the directory reproc used until
  now. These options were added to allow vcpkg to control where the config files
  are installed to.

## 3.0.0

### General

- Removed support for Doxygen (and as a result `REPROC_DOCS`).

  All the Doxygen directives made the header docstrings rather hard to read
  directly. Doxygen's output was also too complicated for a simple library such
  as reproc. Finally, Doxygen doesn't really provide any intuitive support for
  documenting a set of libraries. I have an idea for a Doxygen alternative using
  libclang and cmark but I'm not sure when I'll be able to implement it.

### CMake

- Renamed `REPROCXX` option to `REPROC++`.

  `REPROCXX` was initially chosen because CMake didn't recommend using anything
  other than letters and underscores for variable names. However, `REPROC++`
  turns out to work without any problems so we use it since it's the expected
  name for an option to build reproc++.

- Stopped modifying the default `CMAKE_INSTALL_PREFIX` on Windows.

  In 2.0.0, when installing to the default `CMAKE_INSTALL_PREFIX`, you would end
  up with `C:\Program Files (x86)\reproc` and `C:\Program Files (x86)\reproc++`
  when installing reproc. In 3.0.0, the default `CMAKE_INSTALL_PREFIX` isn't
  modified anymore and all libraries are installed to `CMAKE_INSTALL_PREFIX` in
  exactly the same way as they are on UNIX systems (include and lib
  subdirectories directly beneath the installation directory). Sticking to the
  defaults makes it easy to include reproc in various package managers such as
  vcpkg.

### reproc

- `reproc_terminate` and `reproc_kill` don't call `reproc_wait` internally
  anymore. `reproc_stop` has been changed to call `reproc_wait` after calling
  `reproc_terminate` or `reproc_kill` so it still behaves the same.

  Previously, calling `reproc_terminate` on a list of processes would only call
  `reproc_terminate` on the next process after the previous process had exited
  or the timeout had expired. This made terminating multiple processes take
  longer than required. By removing the `reproc_wait` call from
  `reproc_terminate`, users can first call `reproc_terminate` on all processes
  before waiting for each of them with `reproc_wait` which makes terminating
  multiple processes much faster.

- Default to using `vfork` instead of `fork` on POSIX systems.

  This change was made to increase `reproc_start`'s performance when the parent
  process is using a large amount of memory. In these scenario's, `vfork` can be
  a lot faster than `fork`. Care is taken to make sure signal handlers in the
  child don't corrupt the state of the parent process. This change induces an
  extra constraint in that `set*id` functions cannot be called while a call to
  `reproc_start` is in process, but this situation is rare enough that the
  tradeoff for better performance seems worth it.

  A dependency on pthreads had to be added in order to safely use `vfork` (we
  needed access to `pthread_sigmask`). The CMake and pkg-config files have been
  updated to automatically find pthreads so users don't have to find it
  themselves.

- Renamed `reproc_error_to_string` to `reproc_strerror`.

  The C standard library has `strerror` for retrieving a string representation
  of an error. By using the same function name (prefixed with reproc) for a
  function that does the same for reproc's errors, new users will immediately
  know what the function does.

### reproc++

- reproc++ now takes timeouts as `std::chrono::duration` values (more specific
  `reproc::milliseconds`) instead of unsigned ints.

  Taking the `reproc::milliseconds` type explains a lot more about the expected
  argument than taking an unsigned int. C++14 also added chrono literals which
  make constructing `reproc::milliseconds` values a lot more concise
  (`reproc::milliseconds(2000)` => `2000ms`).
