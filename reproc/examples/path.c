#include <reproc/run.h>

#include <stdlib.h>

// Redirects the output of the given command to the reproc.out file.
int main(int argc, const char *argv[])
{
  (void) argc;

  int r = reproc_run(argv + 1,
                     (reproc_options){ .redirect.path = "reproc.out" });

  if (r < 0) {
    fprintf(stderr, "%s\n", reproc_strerror(r));
  }

  return r < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
