/*! \example forward.cpp */

#include <reproc++/reproc.hpp>
#include <reproc++/sink.hpp>

#include <future>
#include <iostream>

int fail(std::error_code ec)
{
  std::cerr << ec.message();
  return 1;
}

/*
See cmake-help for a completely documented C++ example. We only thorougly
document specifics in this example that are different from what's already been
said in cmake-help.

Forwards the program arguments to a child process and prints its output on
stdout.

Example: "./forward cmake --help" will print CMake's help output.

This program can be used to verify that manually executing a command and
executing it with reproc give the same output.
*/
int main(int argc, char *argv[])
{
  if (argc <= 1) {
    std::cerr << "No arguments provided. Example usage: ./forward cmake --help";
    return 1;
  }

  /*
  The destructor calls process::stop with the parameters we pass in the
  constructor which helps us make sure the process is always stopped correctly
  if unexpected errors happen.

  Any kind of process can be started with forward so we make sure the process
  is cleaned up correctly by specifying reproc::terminate which sends SIGTERM
  (POSIX) or CTRL-BREAK (Windows) and waits 5 seconds. We also add the
  reproc::kill flag which sends SIGKILL (POSIX) or calls TerminateProcess
  (Windows) if the process hasn't exited after 5 seconds and waits 2 more
  seconds for the child process to exit.

  Note that the timout values are maximum wait times. If the process exits
  earlier the destructor will return immediately.
  */
  reproc::process forward(reproc::terminate, 5000, reproc::kill, 2000);

  std::error_code ec = forward.start(argc - 1, argv + 1);

  if (ec == reproc::errc::file_not_found) {
    std::cerr << "Program not found. Make sure it's available from the PATH.";
    return 1;
  }

  if (ec) { return fail(ec); }

  // Some programs wait for the input stream to be closed before continuing so
  // we close it explicitly.
  forward.close(reproc::stream::in);

  /* To avoid the child process hanging because the error stream is full while
  we're waiting for output from the output stream or vice-versa we spawn two
  separate threads to read from both streams at the same time. */

  // Pipe child process stdout output to std::cout of parent process.
  auto drain_stdout = std::async(std::launch::async, [&forward]() {
    return forward.drain(reproc::stream::out, reproc::ostream_sink(std::cout));
  });

  // Pipe child process stderr output to std::cerr of parent process.
  auto drain_stderr = std::async(std::launch::async, [&forward]() {
    return forward.drain(reproc::stream::err, reproc::ostream_sink(std::cerr));
  });

  /* Call stop ourselves to get the exit_status. We add reproc::wait with a
  timeout of 10 seconds to give the process time to write its output before
  sending SIGTERM. */
  unsigned int exit_status = 0;
  ec = forward.stop(reproc::wait, 10000, reproc::terminate, 5000, reproc::kill,
                    2000, &exit_status);
  if (ec) { return fail(ec); }

  ec = drain_stdout.get();
  if (ec) { return fail(ec); }

  ec = drain_stderr.get();
  if (ec) { return fail(ec); }

  return static_cast<int>(exit_status);
}
