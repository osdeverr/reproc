#include <doctest.h>
#include <reproc/reproc.h>

#include <array>

// NOLINTNEXTLINE(cert-err58-cpp)
TEST_CASE("stop")
{
  reproc_type reproc;

  REPROC_ERROR error = REPROC_SUCCESS;
  CAPTURE(error);

  error = reproc_init(&reproc);
  REQUIRE(!error);

  std::array<const char *, 2> argv = { { REPROC_INFINITE_HELPER, nullptr } };
  auto argc = static_cast<int>(argv.size() - 1);

  error = reproc_start(&reproc, argc, argv.data(), nullptr);
  REQUIRE(!error);

// Wait to avoid terminating the child process on Windows before it is
// initialized (which would result in an error window appearing)
#if defined(_WIN32)
  error = reproc_wait(&reproc, 50);
  REQUIRE((error == REPROC_WAIT_TIMEOUT));
#endif

  SUBCASE("terminate")
  {
    error = reproc_terminate(&reproc, 50);
    REQUIRE(!error);
  }

  SUBCASE("kill")
  {
    error = reproc_kill(&reproc, 50);
    REQUIRE(!error);
  }

  error = reproc_destroy(&reproc);
  REQUIRE(!error);
}
