#pragma once

#include <reproc++/reproc.hpp>

#include <iosfwd>
#include <mutex>
#include <ostream>
#include <string>

namespace reproc {
namespace sink {

/*! Reads all output into `out`. */
class string {
  std::string &out_;
  std::string &err_;

public:
  explicit string(std::string &out, std::string &err) noexcept
      : out_(out), err_(err)
  {}

  bool operator()(stream stream, const uint8_t *buffer, size_t size)
  {
    switch (stream) {
      case stream::out:
        out_.append(reinterpret_cast<const char *>(buffer), size);
        break;
      case stream::err:
        err_.append(reinterpret_cast<const char *>(buffer), size);
        break;
      case stream::in:
        break;
    }

    return true;
  }
};

/*! Forwards all output to `out`. */
class ostream {
  std::ostream &out_;
  std::ostream &err_;

public:
  explicit ostream(std::ostream &out, std::ostream &err) noexcept

      : out_(out), err_(err)
  {}

  bool operator()(stream stream, const uint8_t *buffer, size_t size)
  {
    switch (stream) {
      case stream::out:
        out_.write(reinterpret_cast<const char *>(buffer),
                   static_cast<std::streamsize>(size));
        break;
      case stream::err:
        err_.write(reinterpret_cast<const char *>(buffer),
                   static_cast<std::streamsize>(size));
        break;
      case stream::in:
        break;
    }

    return true;
  }
};

/*! Discards all output. */
class discard {
public:
  bool operator()(stream stream, const uint8_t *buffer, size_t size) noexcept
  {
    (void) stream;
    (void) buffer;
    (void) size;

    return true;
  }
};

namespace thread_safe {

/*! `sink::string` but locks the given mutex before appending to the string. */
class string {
  sink::string sink_;
  std::mutex &mutex_;

public:
  string(std::string &out, std::string &err, std::mutex &mutex) noexcept
      : sink_(out, err), mutex_(mutex)
  {}

  bool operator()(stream stream, const uint8_t *buffer, size_t size)
  {
    std::lock_guard<std::mutex> lock(mutex_);
    return sink_(stream, buffer, size);
  }
};

}

}
}
