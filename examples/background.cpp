/*! \example background.cpp */

#include <reproc++/reproc.hpp>

#include <future>
#include <iostream>
#include <mutex>

int fail(std::error_code ec)
{
  std::cerr << ec.message();
  return 1;
}

// Expands upon reproc++'s string parser by locking the given mutex before
// appending new output to the given string.
class thread_safe_string_parser
{
public:
  thread_safe_string_parser(std::string &out, std::mutex &mutex)
      : out_(out), mutex_(mutex)
  {
  }

  bool operator()(const char *buffer, unsigned int size)
  {
    std::lock_guard<std::mutex> lock(mutex_);
    out_.append(buffer, size);
    return true;
  }

private:
  std::string &out_;
  std::mutex &mutex_;
};

/*
The background example reads the output of a child process in a background
thread and shows how to access the current output in the main thread while the
background thread is still running.

Like the forward example it forwards the program arguments to a child process
and prints its output on stdout.
*/
int main(int argc, char *argv[])
{
  reproc::process background(reproc::terminate, 5000, reproc::kill, 2000);

  std::error_code ec = background.start(argc - 1, argv + 1);
  if (ec) { return fail(ec); }

  /* We need a lock along with the output string to prevent the main thread and
  background thread from modifying the string at the same time (std::string
  is not thread safe). */
  std::string output;
  std::mutex mutex;

  auto read_async = std::async(std::launch::async, [&background, &output,
                                                    &mutex]() {
    thread_safe_string_parser parser(output, mutex);
    return background.read(reproc::stream::out, parser);
  });

  // Show new output every 2 seconds.
  while (read_async.wait_for(std::chrono::seconds(2)) !=
         std::future_status::ready) {
    std::lock_guard<std::mutex> lock(mutex);
    std::cout << output;
    // Clear output that's already been flushed to std::cout.
    output.clear();
  }

  // Flush the remaining output of the child process.
  std::cout << output;

  // Check if any errors occurred in the background thread.
  ec = read_async.get();
  if (ec) { return fail(ec); }

  /* Only the background thread has stopped by this point. We can't be certain
  the child process has stopped as well. Because we can't be sure what process
  we're running (the child process to run is determined by the user) we send a
  SIGTERM signal and SIGKILL if necessary (or the Windows equivalents). */
  unsigned int exit_status;
  ec = background.stop(reproc::terminate, 5000, reproc::kill, 2000,
                       &exit_status);
  if (ec) { return fail(ec); }

  return static_cast<int>(exit_status);
}
