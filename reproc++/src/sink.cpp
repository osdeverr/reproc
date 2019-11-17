#include <reproc++/sink.hpp>

#include <ostream>

namespace reproc {
namespace sink {

string::string(std::string &out, std::string &err) noexcept
    : out_(out), err_(err)
{}

bool string::operator()(stream stream, const uint8_t *buffer, unsigned int size)
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

ostream::ostream(std::ostream &out, std::ostream &err) noexcept
    : out_(out), err_(err)
{}

bool ostream::operator()(stream stream,
                         const uint8_t *buffer,
                         unsigned int size)
{
  switch (stream) {
    case stream::out:
      out_.write(reinterpret_cast<const char *>(buffer), size);
      break;
    case stream::err:
      err_.write(reinterpret_cast<const char *>(buffer), size);
      break;
    case stream::in:
      break;
  }

  return true;
}

bool discard::operator()(stream stream,
                         const uint8_t *buffer,
                         unsigned int size) noexcept
{
  (void) stream;
  (void) buffer;
  (void) size;

  return true;
}

namespace thread_safe {

string::string(std::string &out, std::string &err, std::mutex &mutex) noexcept
    : sink_(out, err), mutex_(mutex)
{}

bool string::operator()(stream stream, const uint8_t *buffer, unsigned int size)
{
  std::lock_guard<std::mutex> lock(mutex_);
  return sink_(stream, buffer, size);
}

} // namespace thread_safe

} // namespace sink
} // namespace reproc
