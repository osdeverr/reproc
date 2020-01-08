#include <reproc/sink.h>

#include "error.h"
#include "macro.h"

#include <stdlib.h>
#include <string.h>

int reproc_drain(reproc_t *process, reproc_sink out, reproc_sink err)
{
  assert_return(process, REPROC_EINVAL);
  assert_return(out.function, REPROC_EINVAL);
  assert_return(err.function, REPROC_EINVAL);

  const uint8_t initial = 0;

  // A single call to `read` might contain multiple messages. By always calling
  // both sinks once with no data before reading, we give them the chance to
  // process all previous output one by one before reading from the child
  // process again.
  if (!out.function(REPROC_STREAM_IN, &initial, 0, out.context) ||
      !err.function(REPROC_STREAM_IN, &initial, 0, err.context)) {
    return 0;
  }

  uint8_t buffer[4096];
  int r = -1;

  while (true) {
    REPROC_STREAM stream = { 0 };
    r = reproc_read(process, &stream, buffer, ARRAY_SIZE(buffer));
    if (r < 0) {
      break;
    }

    size_t bytes_read = (size_t) r;

    reproc_sink sink = stream == REPROC_STREAM_OUT ? out : err;

    // `sink` returns false to tell us to stop reading.
    if (!sink.function(stream, buffer, bytes_read, sink.context)) {
      break;
    }
  }

  return r == REPROC_EPIPE ? 0 : r;
}

static bool sink_string(REPROC_STREAM stream,
                        const uint8_t *buffer,
                        size_t size,
                        void *context)
{
  (void) stream;

  char **string = (char **) context;
  size_t string_size = *string == NULL ? 0 : strlen(*string);

  char *realloc_result = (char *) realloc(*string, string_size + size + 1);
  if (realloc_result == NULL) {
    free(*string);
    *string = NULL;
    return false;
  } else {
    *string = realloc_result;
  }

  memcpy(*string + string_size, buffer, size);

  (*string)[string_size + size] = '\0';

  return true;
}

reproc_sink reproc_sink_string(char **output)
{
  return (reproc_sink){ sink_string, output };
}

static bool sink_discard(REPROC_STREAM stream,
                         const uint8_t *buffer,
                         size_t size,
                         void *context)
{
  (void) stream;
  (void) buffer;
  (void) size;
  (void) context;

  return true;
}

reproc_sink reproc_sink_discard(void)
{
  return (reproc_sink){ sink_discard, NULL };
}

const reproc_sink REPROC_SINK_NULL = { sink_discard, NULL };

void *reproc_free(void *ptr)
{
  assert_return(ptr, NULL);
  free(ptr);
  return NULL;
}
