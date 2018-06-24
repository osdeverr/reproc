#include <doctest/doctest.h>
#include <process.h>
#include <string.h>

/* Alternates between writing to stdin and reading from stdout and writing to
 * stdin and reading from stderr.
 * See res/echo for the child process code
 */
TEST_CASE("read-write")
{
  const char *argv[2] = {ECHO_PATH, 0};
  int argc = 1;
  uint32_t timeout = 100;

  const char *stdout_msg = "stdout\n";
  const char *stderr_msg = "stderr\n";

  char buffer[1000];
  uint32_t actual = 0;
  uint32_t to_read;
  char *current;

  Process *process = NULL;
  PROCESS_LIB_ERROR error = process_init(&process);
  REQUIRE(!error);
  REQUIRE(process);

  error = process_start(process, argc, argv, NULL);
  REQUIRE(!error);

  error = process_write(process, stdout_msg, (uint32_t) strlen(stdout_msg),
                        &actual);
  REQUIRE(!error);

  to_read = (uint32_t) strlen(stdout_msg);
  current = buffer;
  while (to_read != 0) {
    error = process_read(process, current, to_read, &actual);
    REQUIRE(!error);
    to_read -= actual;
    current += actual;
  }

  *current = '\0';
  CHECK_EQ(buffer, stdout_msg);

  error = process_write(process, stderr_msg, (uint32_t) strlen(stderr_msg),
                        &actual);
  REQUIRE(!error);

  to_read = (uint32_t) strlen(stderr_msg);
  current = buffer;
  while (to_read != 0) {
    error = process_read_stderr(process, current, to_read, &actual);
    REQUIRE(!error);
    to_read -= actual;
    current += actual;
  }

  *current = '\0';
  CHECK_EQ(buffer, stderr_msg);

  error = process_write(process, stdout_msg, (uint32_t) strlen(stdout_msg),
                        &actual);
  REQUIRE(!error);

  to_read = (uint32_t) strlen(stdout_msg);
  current = buffer;
  while (to_read != 0) {
    error = process_read(process, current, to_read, &actual);
    REQUIRE(!error);
    to_read -= actual;
    current += actual;
  }

  *current = '\0';
  CHECK_EQ(buffer, stdout_msg);

  error = process_write(process, stderr_msg, (uint32_t) strlen(stderr_msg),
                        &actual);
  REQUIRE(!error);

  to_read = (uint32_t) strlen(stderr_msg);
  current = buffer;
  while (to_read != 0) {
    error = process_read_stderr(process, current, to_read, &actual);
    REQUIRE(!error);
    to_read -= actual;
    current += actual;
  }

  *current = '\0';
  CHECK_EQ(buffer, stderr_msg);

  error = process_wait(process, timeout);
  REQUIRE(!error);

  int32_t exit_status;
  error = process_exit_status(process, &exit_status);
  REQUIRE(!error);
  REQUIRE((exit_status == 0));

  error = process_free(&process);
  REQUIRE(!error);
}