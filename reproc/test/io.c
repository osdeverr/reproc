#include "assert.h"

#include <reproc/reproc.h>
#include <reproc/sink.h>

#include <string.h>

#define MESSAGE "reproc stands for REdirected PROCess"

static void io()
{
  int r = -1;

  reproc_t *process = reproc_new();
  assert(process);

  const char *argv[3] = { RESOURCE_DIRECTORY "/io", NULL };

  r = reproc_start(process, argv, (reproc_options){ 0 });
  assert(r >= 0);

  r = reproc_write(process, (uint8_t *) MESSAGE, strlen(MESSAGE));
  assert(r == strlen(MESSAGE));

  r = reproc_close(process, REPROC_STREAM_IN);
  assert(r == 0);

  char *out = NULL;
  char *err = NULL;
  r = reproc_drain(process, reproc_sink_string(&out), reproc_sink_string(&err));
  assert(r == 0);

  assert(out != NULL);
  assert(err != NULL);

  assert(strcmp(out, MESSAGE) == 0);
  assert(strcmp(err, MESSAGE) == 0);

  r = reproc_wait(process, REPROC_INFINITE);
  assert(r == 0);

  reproc_destroy(process);

  reproc_free(out);
  reproc_free(err);
}

static void timeout(void)
{
  int r = -1;

  reproc_t *process = reproc_new();
  assert(process);

  const char *argv[3] = { RESOURCE_DIRECTORY "/io", "stdout", NULL };

  r = reproc_start(process, argv, (reproc_options){ .timeout = 200 });
  assert(r >= 0);

  uint8_t buffer = 0;
  r = reproc_read(process, NULL, &buffer, sizeof(buffer));
  assert(r == REPROC_ETIMEDOUT);

  r = reproc_close(process, REPROC_STREAM_IN);
  assert(r == 0);

  r = reproc_read(process, NULL, &buffer, sizeof(buffer));
  assert(r == REPROC_EPIPE);

  reproc_destroy(process);
}

int main(void)
{
  io();
  timeout();
}
