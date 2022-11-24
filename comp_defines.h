#ifndef __COMP_STRUCTS__
#define __COMP_STRUCTS__

#include <stdlib.h>
#include <sys/types.h>

#define TIME_LIMIT 1  // determine time limit
#define LINE_BUFF_SIZE 512
#define IO_FILE_SIZE 4096
#define IO_MAX_LINES 50
#define SIZE_IO_LINE sizeof(struct io_comp_line)
#define SIZE_IO_LINE_PTR sizeof(struct io_comp_line *)
#define SIZE_IO_MAX_PTR SIZE_IO_LINE_PTR *IO_MAX_LINES

enum testCaseType { none = 0, OrdIO = 1, Brute = 2 };

enum io_line_type {
  io_line_none = 0,
  io_line_out = 1,
  io_line_req = 2,
  io_line_in = 3
};

struct io_comp_line {
  enum io_line_type type;
  char line[LINE_BUFF_SIZE];
  size_t length;
};

struct testCase {
  enum testCaseType type;
  char test_file[LINE_BUFF_SIZE];
  char prog_file[LINE_BUFF_SIZE];
  char *args[LINE_BUFF_SIZE];
  size_t ret_val_expect;
  size_t ret_val_got;
  int is_protected;
  float points;
  float maxPoints;
};

struct tests {
  const char *name[LINE_BUFF_SIZE];
  struct testCase *testCases;
  float points;
  float maxPoints;
};

#endif
