#include "reproc/reproc.hpp"

#include <reproc/reproc.h>

#include <ostream>

const unsigned int Reproc::INFINITE = REPROC_INFINITE;

Reproc::Reproc() : reproc(new struct reproc()) {}

Reproc::~Reproc()
{
  // Check for null because the object could have been moved
  if (reproc) { reproc_destroy(reproc.get()); }
}

Reproc::Error Reproc::start(int argc, const char *argv[],
                            const char *working_directory)
{
  return static_cast<Reproc::Error>(
      reproc_start(reproc.get(), argc, argv, working_directory));
}

Reproc::Error Reproc::start(const std::vector<std::string> &args,
                            const std::string *working_directory)
{
  // Turn args into array of C strings
  auto argv = std::vector<const char *>(args.size() + 1);

  for (std::size_t i = 0; i < args.size(); i++) {
    argv[i] = args[i].c_str();
  }
  argv[args.size()] = nullptr;

  // We don't expect so many args that an int will not be sufficient to count
  // them
  auto argc = static_cast<int>(args.size());

  Error error = start(argc, &argv[0],
                      working_directory ? working_directory->c_str() : nullptr);

  return error;
}

Reproc::Error Reproc::close_stdin()
{
  return static_cast<Reproc::Error>(reproc_close_stdin(reproc.get()));
}

Reproc::Error Reproc::write(const void *buffer, unsigned int to_write,
                            unsigned int *bytes_written)
{
  return static_cast<Reproc::Error>(
      reproc_write(reproc.get(), buffer, to_write, bytes_written));
}

Reproc::Error Reproc::read(void *buffer, unsigned int size,
                           unsigned int *bytes_read)
{
  return static_cast<Reproc::Error>(
      reproc_read(reproc.get(), buffer, size, bytes_read));
}

Reproc::Error Reproc::read_stderr(void *buffer, unsigned int size,
                                  unsigned int *bytes_read)
{
  return static_cast<Reproc::Error>(
      reproc_read_stderr(reproc.get(), buffer, size, bytes_read));
}

Reproc::Error Reproc::read_all(std::ostream &out)
{
  return read_all([&out](const char *buffer, unsigned int size) {
    out.write(buffer, size);
  });
}

Reproc::Error Reproc::read_all_stderr(std::ostream &out)
{
  return read_all_stderr([&out](const char *buffer, unsigned int size) {
    out.write(buffer, size);
  });
}

Reproc::Error Reproc::read_all(std::string &out)
{
  return read_all([&out](const char *buffer, unsigned int size) {
    out.append(buffer, size);
  });
}

Reproc::Error Reproc::read_all_stderr(std::string &out)
{
  return read_all_stderr([&out](const char *buffer, unsigned int size) {
    out.append(buffer, size);
  });
}

Reproc::Error Reproc::wait(unsigned int milliseconds)
{
  return static_cast<Reproc::Error>(reproc_wait(reproc.get(), milliseconds));
}

Reproc::Error Reproc::terminate(unsigned int milliseconds)
{
  return static_cast<Reproc::Error>(
      reproc_terminate(reproc.get(), milliseconds));
}

Reproc::Error Reproc::kill(unsigned int milliseconds)
{
  return static_cast<Reproc::Error>(reproc_kill(reproc.get(), milliseconds));
}

Reproc::Error Reproc::exit_status(int *exit_status)
{
  return static_cast<Reproc::Error>(
      reproc_exit_status(reproc.get(), exit_status));
}

unsigned int Reproc::system_error() { return reproc_system_error(); }

std::string Reproc::error_to_string(Reproc::Error error)
{
  if (error == Reproc::UNKNOWN_ERROR) {
    return "reproc => unknown error. system error = " +
           std::to_string(system_error());
  }
  return reproc_error_to_string(static_cast<REPROC_ERROR>(error));
}
