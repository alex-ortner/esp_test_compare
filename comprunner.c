#include <errno.h>
#include <malloc.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define TIME_LIMIT 1  // determine time limit
#define LINE_BUFF_SIZE 512
#define IO_FILE_SIZE 4096

enum testCaseType { none = 0, OrdIO = 1, Brute = 2 };

enum io_line_type { line_out = 0, line_req = 1, line_in = 2 };

struct io_comp_line {
  enum io_line_type type;
  char line[LINE_BUFF_SIZE];
  size_t length;
};

struct testCase {
  enum testCaseType type;
  char test_file[LINE_BUFF_SIZE];
  char prog_file[LINE_BUFF_SIZE];
  char args[5][LINE_BUFF_SIZE];
  size_t ret_val_expect;
  size_t ret_val_got;
  int protected;
  float points;
  float maxPoints;
};

void alarm_handler(int);
void child_handler(int);

/* child after forking */
int child();

/* parent after forking */
int parent(int, int[2], int[2]);

/* Compares str1 with str2 */
int comp(const char* str1, const char* str2, int buff_len);

/* Searches str1 for str2 */
int strsrch(const char* str1, const char* str2, int buff_len);

int io_pipe_handler(int f_stdin[2], int f_stdout[2], char* out_file,
                    int* out_index, struct io_comp_line** responses,
                    int response_count);

int get_io_lines(struct testCase* test, struct io_comp_line** io_lines);

const char out_first[IO_FILE_SIZE];
const char out_second[IO_FILE_SIZE];
size_t out_first_ptr = 0;
size_t out_second_ptr = 0;

struct tests {
  const char* name[LINE_BUFF_SIZE];
  struct testCase* testCases;
  float points;
  float maxPoints;
};

const char* responses[] = {"CODE\n", "EESI\n", "REDN\n", "ENO\n", "YARI\n"};

const int len_responses[] = {5, 5, 5, 4, 5};

// not liking that I have to do that
int timeout = 0;
int test_done = 0;

int main(int argc, char** argv) {
  int retValue = 0;
  int fd[2];
  int fd2[2];
  if (pipe(fd) == -1) printf("Error occured while opening pipe");

  if (pipe(fd2) == -1) printf("Error occured while opening pipe");

  if (argc > 1) {
    printf("Got Arguments:\n");
    for (int i = 0; i < argc; i++) {
      printf("  %s\n", argv[i]);
    }
    fflush(stdout);
  }
  printf("Process pid(%d), pipe %d : %d\n", getpid(), fd[0], fd[1]);
  printf("Process pid(%d), pipe %d : %d\n", getpid(), fd2[0], fd2[1]);

  int pid = fork();

  if (pid == 0) {
    /* try to get child to don't buffer stdout */
    setbuf(stdout, NULL);

    /* close unused sides of pipes */
    close(fd[0]);
    close(fd2[1]);

    /* overwrite stdio & stdout */
    dup2(fd[1], 1);
    dup2(fd2[0], 0);

    /* close other pipes */
    close(fd[1]);
    close(fd2[0]);

    /* we should never come back here  */
    retValue = child();
  } else {
    retValue = parent(pid, fd, fd2);
    fflush(stdout);
    close(fd[0]);

    int wstatus;
    int res = wait(&wstatus);
    printf("Wstatus %d\n", wstatus);
    if (WIFEXITED(wstatus)) {
      int status = WEXITSTATUS(wstatus);
      printf("Process %d exited with Code %d\n", res, status);
    }
  }

  return 0;
}

int child() {
  char* args[] = {"./a2", "A", NULL};
  return execv("./a2", args);
}

int parent(int pid, int fd[2], int fd2[2]) {
  char buff[256];
  int respNr = 0;
  int respMax = 5;
  int readLen = 0;
  struct pollfd fds[2];
  int ret;
  close(fd[1]);
  close(fd2[0]);
  printf("From Child\n\n");

  /* watch stdin for input */
  fds[0].fd = fd[0];
  fds[0].events = POLLIN;

  /* watch stdout for ability to write */
  fds[1].fd = fd2[1];
  fds[1].events = POLLOUT;

  while ((ret = poll(fds, 2, 100))) {
    if (!ret) {
      printf("%d miliseconds elapsed.\n", 500);
    }

    if (fds[1].revents & POLLOUT) {
      if (respNr == respMax) {
        close(fd2[1]);
        break;
      }
      write(fd2[1], responses[respNr], len_responses[respNr]);
      respNr++;
    }
  }

  while ((readLen = read(fd[0], buff, sizeof(buff))) > 0) {
    write(1, buff, readLen);
    fflush(stdout);
    for (int i = 0; i < 256; i++) buff[i] = 0;
  }

  printf("\n\nParent Error (%d): %d\n", pid, errno);
  close(fd[0]);
  close(fd2[1]);
  return 0;
}

int comp(const char* str1, const char* str2, int buff_len) {
  for (int i = 0; i < buff_len && str1[i] != '\0' && str2[i] != '\0'; i++) {
    if (str1[i] != str2[i]) return 0;
  }
  return 1;
}

int strsrch(const char* str1, const char* str2, int buff_len) {
  int i = 0;
  int len = -1;
  int pos = -1;
  for (i = 0; i < buff_len && str1[i] != 0; i++)
    ;
  if (i < buff_len)
    len = i;
  else
    return -1;
  do {
    for (i = pos + 1; i < len && str1[i] != str2[0]; i++)
      ;
    if (i == len) return -1;
    pos = i;
    for (i = 0; pos + i < len && str1[pos + i] == str2[i] && str2[i] != 0; i++)
      ;
    if (str2[i] == 0) return pos;
  } while (pos >= 0);
  return -1;
}

int test_runner(struct testCase* test, char* out_file, int* out_index) {
  struct io_comp_line** io_lines = NULL;

  int resp_count = get_io_lines(test, io_lines);
  if (resp_count == -1) {
    free(io_lines);
    return 3;
  }

  int f_stdin[2];
  int f_stdout[2];

  if (pipe(f_stdin) == -1) {
    perror("Failed to open stdin pipe\n");

    free(io_lines);
    return 2;
  }

  if (pipe(f_stdout) == -1) {
    perror("Failed to open stdout pipe\n");
    close(f_stdin[0]);
    close(f_stdin[1]);

    free(io_lines);
    return 2;
  }

  pid_t pid = fork();

  if (pid == -1) {
    perror("fork failed");
    free(io_lines);
    return 1;
  } else if (pid == 0) {
    setbuf(stdout, NULL);
    close(f_stdout[0]);
    close(f_stdin[1]);

    dup2(f_stdout[1], 1);
    dup2(f_stdin[0], 0);

    close(f_stdout[1]);
    close(f_stdin[0]);
    free(io_lines);
    execlp(test->prog_file, test->args[0], test->args[1], test->args[2],
           test->args[3], test->args[4], NULL);
    perror("exec failed");
    return -2;
  }

  // set up the signal handlers after forking so the child doesn't inherit them

  signal(SIGALRM, alarm_handler);
  signal(SIGCHLD, child_handler);

  alarm(TIME_LIMIT);  // install an alarm to be fired after TIME_LIMIT

  int ret_resp = io_pipe_handler(f_stdin, f_stdout, out_file, out_index,
                                 io_lines, resp_count);
  if (ret_resp != 0) {
    free(io_lines);
    return ret_resp;
  }
  if (timeout == 0) pause();

  int wstatus;
  int result = waitpid(pid, &wstatus, WNOHANG);
  if (timeout && result == 0) {
    printf("alarm triggered\n");

    // child still running, so kill it
    printf("killing child\n");
    kill(pid, 9);
    wait(NULL);
    free(io_lines);
    return -1;

  } else if (test_done) {
    printf("Wstatus %d\n", wstatus);
    if (WIFEXITED(wstatus)) {
      int status = WEXITSTATUS(wstatus);
      printf("Process %d exited with Code %d\n", result, status);
      test->ret_val_got = status;
    }
  }
  free(io_lines);
  return 0;
}

void child_handler(int sig) {
  test_done = 1;
  sig += 0;
}

void alarm_handler(int sig) {
  timeout = 1;
  sig += 0;
}

int io_pipe_handler(int f_stdin[2], int f_stdout[2], char* out_file,
                    int* out_index, struct io_comp_line** responses,
                    int response_count) {
  char buff[LINE_BUFF_SIZE];
  int respNr = 0;
  int readLen = 0;
  struct pollfd fds[2];
  int ret;
  close(f_stdout[1]);
  close(f_stdin[0]);
  printf("From Child\n\n");

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
    if (timeout) {
      close(f_stdout[0]);
      close(f_stdin[1]);
      return -1;
    }

    if (fds[1].revents & POLLOUT) {
      if (respNr == response_count) {
        close(f_stdin[1]);
        break;
      }
      write(f_stdin[1], responses[respNr]->line, responses[respNr]->length);
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

int get_io_lines(struct testCase* test, struct io_comp_line** io_lines) {
  FILE* fptr = fopen(test->test_file, "r");
  io_lines = malloc(sizeof(struct io_comp_line));
  int lines = 0;
  char buff[LINE_BUFF_SIZE];
  while (fgets(buff, LINE_BUFF_SIZE, fptr) != NULL) {
    io_lines = realloc(io_lines, sizeof(struct io_comp_line) * lines);
    if (buff[0] == '>')
      io_lines[lines]->type = line_out;
    else if (buff[0] == '<')
      io_lines[lines]->type = line_in;
    else if (buff[0] == '?')
      io_lines[lines]->type = line_req;
    else if (buff[0] == '!')
      break;
    else
      return -1;
    for (int i = 0;
         i + 2 < LINE_BUFF_SIZE && buff[i + 2] != '\n' && buff[i + 2] != '\0';
         i++) {
      io_lines[lines]->line[i] = buff[i + 2];
    }
    lines++;
  }

  return lines;
}
