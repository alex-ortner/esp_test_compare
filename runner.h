#ifndef __COMP_RUNNER__
#define __COMP_RUNNER__

#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "comp_defines.h"
#include "dyn_malloc.h"
#include "io_struct_fun.h"

int __timeout = 0;
int __test_done = 0;

void child_handler(int sig) {
  __test_done = 1;
  sig += 0;
}

void alarm_handler(int sig) {
  __timeout = 1;
  sig += 0;
}

int io_pipe_handler(int f_stdin[2], int f_stdout[2], char *out_file,
                    int *out_index, struct io_comp_line **io_resp,
                    int response_count) {
  char buff[LINE_BUFF_SIZE];
  int respNr = 0;
  int readLen = 0;
  struct pollfd fds[2];
  int ret;
  close(f_stdout[1]);
  close(f_stdin[0]);
  printf("Pipe to Child\n\n");

  /* watch stdin for input */
  fds[0].fd = f_stdout[0];
  fds[0].events = POLLIN;

  /* watch stdout for ability to write */
  fds[1].fd = f_stdin[1];
  fds[1].events = POLLOUT;

  while ((ret = poll(fds, 2, 100))) {
    if (!ret) {
      printf("%d miliseconds elapsed.\n", 100);
    }
    if (__timeout) {
      close(f_stdout[0]);
      close(f_stdin[1]);
      perror("Timeout occured");
      return -1;
    }

    if (fds[1].revents & POLLOUT) {
      if (respNr == response_count) {
        close(f_stdin[1]);
        break;
      }
      write(f_stdin[1], io_resp[respNr]->line, io_resp[respNr]->length);
      respNr++;
    }
  }

  while ((readLen = read(f_stdout[0], buff, sizeof(buff))) > 0) {
    write(1, buff, readLen);
    fflush(stdout);
    int i = 0;
    for (; i < LINE_BUFF_SIZE && i < readLen && buff[i] != 0; i++) {
      out_file[*out_index++] = buff[i];
      buff[i] = 0;
    }
    for (; i < LINE_BUFF_SIZE; i++) buff[i] = 0;
  }
  close(f_stdout[0]);
  close(f_stdin[1]);
  return 0;
}

int test_runner(struct testCase *test, char *out_file, int *out_index) {
  setbuf(stdout, NULL);
  printf("-> Initialise Testrunner:\n");
  struct io_comp_line **io_lines = NULL;
  struct io_comp_line **io_resps = NULL;

  ssize_t line_count = 0;
  io_lines = get_io_lines(test, &line_count);
  printf("-> got File Input:\n");

  if (line_count == -1) {
    free(io_lines);
    free(io_resps);
    return 3;
  }
  // for (long i = 0; i < line_count; i++) printf("> %s", io_lines[i]->line);

  printf("-> start filtering:\n");
  ssize_t resp_count = 0;
  io_resps = filter_io_lines(io_lines, line_count, io_line_in, &resp_count);
  // printf("resp_count: %zd\n", resp_count);
  // for (long i = 0; i < resp_count; i++) printf("> %s", io_resps[i]->line);

  int f_stdin[2];
  int f_stdout[2];

  if (pipe(f_stdin) == -1) {
    perror("Failed to open stdin pipe\n");
    free_array((void **)io_lines, line_count);
    free_array((void **)io_resps, resp_count);
    return 2;
  }

  if (pipe(f_stdout) == -1) {
    perror("Failed to open stdout pipe\n");
    close(f_stdin[0]);
    close(f_stdin[1]);
    free_array((void **)io_lines, line_count);
    free_array((void **)io_resps, resp_count);
    return 2;
  }

  pid_t pid = fork();
  if (pid == -1) {
    perror("fork failed");
    free_array((void **)io_lines, line_count);
    free_array((void **)io_resps, resp_count);
    return 1;
  } else if (pid == 0) {
    printf("-> Child generated\n");
    setbuf(stdout, NULL);
    close(f_stdout[0]);
    close(f_stdin[1]);

    dup2(f_stdout[1], 1);
    dup2(f_stdin[0], 0);

    close(f_stdout[1]);
    close(f_stdin[0]);
    free(io_lines);
    free(io_resps);
    execvp(test->prog_file, test->args);
    perror("exec failed");
    return -2;
  }

  // set up the signal handlers after forking so the child doesn't inherit them

  signal(SIGALRM, alarm_handler);
  signal(SIGCHLD, child_handler);

  sleep(1);

  alarm(TIME_LIMIT);  // install an alarm to be fired after TIME_LIMIT

  int ret_resp = io_pipe_handler(f_stdin, f_stdout, out_file, out_index,
                                 io_resps, resp_count);
  if (ret_resp != 0) {
    perror("response handler failed\n");
    free_array((void **)io_lines, line_count);
    free_array((void **)io_resps, resp_count);
    return ret_resp;
  }
  if (__timeout == 0) pause();

  int wstatus;
  int result = waitpid(pid, &wstatus, WNOHANG);
  if (__timeout && result == 0) {
    free_array((void **)io_lines, line_count);
    free_array((void **)io_resps, resp_count);
    printf("alarm triggered\n");

    // child still running, so kill it
    printf("killing child\n");
    kill(pid, 9);
    wait(NULL);

    return -1;
  } else if (__test_done) {
    printf("\n\nWstatus %d\n", wstatus);
    if (WIFEXITED(wstatus)) {
      int status = WEXITSTATUS(wstatus);
      printf("Process %d exited with Code %d\n", result, status);
      test->ret_val_got = status;
    }
  }
  free_array((void **)io_lines, line_count);
  free_array((void **)io_resps, resp_count);

  return 0;
}

#endif
