#include <reproc++/reproc.hpp>

#include <reproc/reproc.h>

namespace reproc {

namespace signal {

const int kill = REPROC_SIGKILL;
const int terminate = REPROC_SIGTERM;

}

const milliseconds infinite = milliseconds(REPROC_INFINITE);

static std::error_code error_code_from(int r)
{
  if (r >= 0) {
    return {};
  }

  if (r == REPROC_EPIPE) {
    // https://github.com/microsoft/STL/pull/406
    return { static_cast<int>(std::errc::broken_pipe),
             std::generic_category() };
  }

  return { -r, std::system_category() };
}

static reproc_stop_actions reproc_stop_actions_from(stop_actions stop)
{
  return {
    { static_cast<REPROC_STOP>(stop.first.action), stop.first.timeout.count() },
    { static_cast<REPROC_STOP>(stop.second.action),
      stop.second.timeout.count() },
    { static_cast<REPROC_STOP>(stop.third.action), stop.third.timeout.count() }
  };
}

auto deleter = [](reproc_t *process) { reproc_destroy(process); };

process::process() : process_(reproc_new(), deleter) {}
process::~process() noexcept = default;

process::process(process &&other) noexcept = default;
process &process::operator=(process &&other) noexcept = default;

std::error_code process::start(const arguments &arguments,
                               const options &options) noexcept
{
  reproc_options reproc_options = {
    options.environment.data(),
    options.working_directory,
    { static_cast<REPROC_REDIRECT>(options.redirect.in),
      static_cast<REPROC_REDIRECT>(options.redirect.out),
      static_cast<REPROC_REDIRECT>(options.redirect.err) },
    reproc_stop_actions_from(options.stop),
    options.timeout.count(),
    options.inherit,
    options.discard,
    { options.input.data(), options.input.size() }
  };

  int r = reproc_start(process_.get(), arguments.data(), reproc_options);

  return error_code_from(r);
}

std::tuple<stream, size_t, std::error_code> process::read(uint8_t *buffer,
                                                          size_t size) noexcept
{
  REPROC_STREAM stream = {};
  int r = reproc_read(process_.get(), &stream, buffer, size);
  return { static_cast<enum stream>(stream), r, error_code_from(r) };
}

std::error_code process::write(const uint8_t *buffer, size_t size) noexcept
{
  int r = reproc_write(process_.get(), buffer, size);
  return error_code_from(r);
}

std::error_code process::close(stream stream) noexcept
{
  int r = reproc_close(process_.get(), static_cast<REPROC_STREAM>(stream));
  return error_code_from(r);
}

std::pair<int, std::error_code> process::wait(milliseconds timeout) noexcept
{
  int r = reproc_wait(process_.get(), timeout.count());
  return { r, error_code_from(r) };
}

std::error_code process::terminate() noexcept
{
  int r = reproc_terminate(process_.get());
  return error_code_from(r);
}

std::error_code process::kill() noexcept
{
  int r = reproc_kill(process_.get());
  return error_code_from(r);
}

std::pair<int, std::error_code> process::stop(stop_actions stop) noexcept
{
  int r = reproc_stop(process_.get(), reproc_stop_actions_from(stop));
  return { r, error_code_from(r) };
}

}
