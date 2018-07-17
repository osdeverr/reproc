#pragma once

#include <iosfwd>
#include <string>
#include <vector>

class Reproc
{
public:
  static const unsigned int INFINITE;

  /*! \see REPROC_ERROR */
  /* When editing make sure to change the corresponding enum in reproc.h as
   * well */
  enum Error {
    SUCCESS,
    UNKNOWN_ERROR,
    WAIT_TIMEOUT,
    STREAM_CLOSED,
    STILL_RUNNING,
    MEMORY_ERROR,
    PIPE_LIMIT_REACHED,
    INTERRUPTED,
    PROCESS_LIMIT_REACHED,
    INVALID_UNICODE,
    PERMISSION_DENIED,
    SYMLINK_LOOP,
    FILE_NOT_FOUND,
    NAME_TOO_LONG,
    PARTIAL_WRITE
  };

  /*! \see reproc_init. Throws std::bad_alloc if allocating memory for the
  reproc struct of the underlying C library fails */
  Reproc();
  /*! \see reproc_destroy */
  ~Reproc();

  /* Enforce unique ownership */
  Reproc(const Reproc &) = delete;
  Reproc(Reproc &&other) noexcept;
  Reproc &operator=(const Reproc &) = delete;
  Reproc &operator=(Reproc &&other) noexcept;

  /*! \see reproc_start */
  Reproc::Error start(int argc, const char *argv[],
                      const char *working_directory);

  /*!
  Overload of start for convenient usage from C++.

  \param[in] args Has the same restrictions as argv in \see reproc_start except
  that it should not end with NULL (which is added in this function).
  */
  Reproc::Error start(const std::vector<std::string> &args,
                      const std::string *working_directory);

  /*! \see reproc_write */
  Reproc::Error write(const void *buffer, unsigned int to_write,
                      unsigned int *bytes_written);

  /*! \see reproc_close_stdin */
  Reproc::Error close_stdin();

  /*! \see reproc_read */
  Reproc::Error read(void *buffer, unsigned int size, unsigned int *bytes_read);

  /*! \see reproc_read_stderr */
  Reproc::Error read_stderr(void *buffer, unsigned int size,
                            unsigned int *bytes_read);

  Reproc::Error read_all(std::ostream &out);
  Reproc::Error read_all_stderr(std::ostream &out);
  Reproc::Error read_all(std::string &out);
  Reproc::Error read_all_stderr(std::string &out);

  /*!
  Calls \see read until the child process stdout is closed or an error occurs.

  Takes a Write function with the following signature:

  \code{.cpp}
  void write(const char *buffer, unsigned int size);
  \endcode

  The Write function receives the buffer after each read so it can append it to
  the final result.

  Example usage (don't actually use this, use /see read_all instead):

  \code{.cpp}
  std::string output;
  reproc.read_all([&output](const char *buffer, unsigned int size) {
    output.append(buffer, size);
  });
  \endcode

  \return Reproc::Error \see reproc_read except for STREAM_CLOSED
  */
  template <class Write> Reproc::Error read_all_generic(Write &&write)
  {
    return read_all(
        [this](void *buffer, unsigned int size, unsigned int *bytes_read) {
          return read(buffer, size, bytes_read);
        },
        std::forward<Write>(write));
  }

  /*! \see read_all_generic for stderr */
  template <class Write> Reproc::Error read_all_stderr_generic(Write &&write)
  {
    return read_all(
        [this](void *buffer, unsigned int size, unsigned int *bytes_read) {
          return read_stderr(buffer, size, bytes_read);
        },
        std::forward<Write>(write));
  }

  /*! \see reproc_wait */
  Reproc::Error wait(unsigned int milliseconds);

  /*! \see reproc_terminate */
  Reproc::Error terminate(unsigned int milliseconds);

  /*! \see reproc_kill */
  Reproc::Error kill(unsigned int milliseconds);

  /*! \see reproc_exit_status */
  Reproc::Error exit_status(int *exit_status);

  /*! \see reproc_system_error */
  static unsigned int system_error();

  static std::string error_to_string(Reproc::Error error);

private:
  struct reproc *reproc;

  static constexpr unsigned int BUFFER_SIZE = 1024;

  template <class Read, class Write>
  Reproc::Error read_all(Read &&read, Write &&write)
  {
    char buffer[BUFFER_SIZE];
    Reproc::Error error = Reproc::SUCCESS;

    while (true) {
      unsigned int bytes_read = 0;
      error = read(buffer, BUFFER_SIZE, &bytes_read);
      if (error) { break; }

      write(buffer, bytes_read);
    }

    if (error != Reproc::STREAM_CLOSED) { return error; }

    return Reproc::SUCCESS;
  }
};
