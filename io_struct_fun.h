#ifndef __IO_STRUCT_FUNC__
#define __IO_STRUCT_FUNC__

#include <stdio.h>
#include <stdlib.h>

#include "comp_defines.h"

struct io_comp_line **get_io_lines(struct testCase *test, long *line_count) {
  FILE *fptr = fopen(test->test_file, "r");
  struct io_comp_line **io_lines = NULL;
  if (fptr == NULL) {
    perror("Could not open file! ");
    return NULL;
  }
  *line_count = 0;
  char c;
  for (c = getc(fptr); c != EOF; c = getc(fptr))
    if (c == '\n')  // Increment count if this character is newline
      (*line_count)++;

  printf("  -> Get Memory via Malloc \n");
  io_lines = (struct io_comp_line **)n_malloc_array(
      sizeof(struct io_comp_line *), sizeof(struct io_comp_line), *line_count);
  if (io_lines == NULL) {
    perror("Failed to allocate memory\n");
    return NULL;
  }
  printf("  -> Finished allocating %p \n", (void *)io_lines);

  long lines = 0;

  fseek(fptr, 0, SEEK_SET);
  char buff[LINE_BUFF_SIZE];

  printf("  -> start reading file %p\n", (void *)fptr);
  while (fgets(buff, LINE_BUFF_SIZE, fptr) != NULL) {
    io_lines[lines]->type = io_line_none;
    if (buff[0] == '>')
      io_lines[lines]->type = io_line_out;
    else if (buff[0] == '<')
      io_lines[lines]->type = io_line_in;
    else if (buff[0] == '?')
      io_lines[lines]->type = io_line_req;
    else if (buff[0] == '!')
      break;
    else {
      fclose(fptr);
      return NULL;
    }
    int i = 0;
    for (; i + 2 < LINE_BUFF_SIZE && buff[i + 2] != '\n' && buff[i + 2] != '\0';
         i++) {
      io_lines[lines]->line[i] = buff[i + 2];
    }
    io_lines[lines]->line[i] = '\n';
    io_lines[lines]->length = i + 1;
    for (i = i + 1; i < LINE_BUFF_SIZE; i++) {
      io_lines[lines]->line[i] = 0;
    }
    lines++;
  }
  fclose(fptr);
  return io_lines;
}

struct io_comp_line **filter_io_lines(struct io_comp_line **io_lines,
                                      int line_count, enum io_line_type filter,
                                      ssize_t *out_line_count) {
  *out_line_count = 0;
  for (int i = 0; i < line_count; i++) {
    if (io_lines[i]->type == filter) {
      (*out_line_count)++;
    }
  }

  struct io_comp_line **out_io_lines = (struct io_comp_line **)n_malloc_array(
      sizeof(struct io_comp_line *), sizeof(struct io_comp_line),
      *out_line_count);
  if (out_io_lines == NULL) return NULL;

  int lines = 0;
  for (int i = 0; i < line_count; i++) {
    if (io_lines[i]->type == filter) {
      size_t j = 0;
      for (; j < io_lines[i]->length; j++) {
        out_io_lines[lines]->line[j] = io_lines[i]->line[j];
      }
      out_io_lines[lines]->line[j] = '\n';
      out_io_lines[lines]->length = j + 1;
      for (j = j + 1; j < LINE_BUFF_SIZE; j++) {
        out_io_lines[lines]->line[j] = 0;
      }
      lines++;
    }
  }
  return out_io_lines;
}

#endif
