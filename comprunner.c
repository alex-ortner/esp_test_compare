#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "comp_defines.h"
#include "dyn_malloc.h"
#include "io_struct_fun.h"
#include "runner.h"

// not liking that I have to do that
char out_first[IO_FILE_SIZE];
char out_second[IO_FILE_SIZE];
int out_first_ptr = 0;
int out_second_ptr = 0;

int main(int argc, char **argv) {
  struct testCase test_case = {
      OrdIO, "test/10/io", "./a2", {"./a2", "E", NULL}, 0, 0, 0, 0.0f, 10.0f};

  if (argc > 1) {
    strcpy(test_case.test_file, argv[1]);
  }
  printf("Start process with file %s:\n", test_case.test_file);
  int test_res = test_runner(&test_case, out_first, &out_first_ptr);
  printf("Testresult : %d\n\n", test_res);
  return 0;
}
