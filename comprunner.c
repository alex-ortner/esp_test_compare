#define _POSIX_C_SOURCE 200809L
#include <stdio.h>

#include <errno.h>
//#include <malloc.h>
#include <signal.h>

#include <string.h>
#include <sys/poll.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>

#define TIME_LIMIT 1 // determine time limit
#define LINE_BUFF_SIZE 512
#define IO_FILE_SIZE 4096
#define IO_MAX_LINES 50
#define SIZE_IO_LINE sizeof(struct io_comp_line)
#define SIZE_IO_LINE_PTR sizeof(struct io_comp_line *)
#define SIZE_IO_MAX_PTR SIZE_IO_LINE_PTR * IO_MAX_LINES

enum testCaseType
{
  none = 0,
  OrdIO = 1,
  Brute = 2
};

enum io_line_type
{
  line_out = 0,
  line_req = 1,
  line_in = 2
};

struct io_comp_line
{
  enum io_line_type type;
  char line[LINE_BUFF_SIZE];
  size_t length;
};

struct testCase
{
  enum testCaseType type;
  char test_file[LINE_BUFF_SIZE];
  char prog_file[LINE_BUFF_SIZE];
  char *args[LINE_BUFF_SIZE];
  size_t ret_val_expect;
  size_t ret_val_got;
  int protected;
  float points;
  float maxPoints;
};

struct tests
{
  const char *name[LINE_BUFF_SIZE];
  struct testCase *testCases;
  float points;
  float maxPoints;
};

// not liking that I have to do that
int timeout = 0;
int test_done = 0;

char out_first[IO_FILE_SIZE];
char out_second[IO_FILE_SIZE];
int out_first_ptr = 0;
int out_second_ptr = 0;

const char *responses[] = {"CODE\n", "EESI\n", "REDN\n", "ENO\n", "YARI\n"};
const int len_responses[] = {5, 5, 5, 4, 5};

void alarm_handler(int);
void child_handler(int);

int comp(const char *str1, const char *str2, int buff_len);
int strsrch(const char *str1, const char *str2, int buff_len);
int test_runner(struct testCase *test, char *out_file, int *out_index);
int io_pipe_handler(int f_stdin[2], int f_stdout[2], char *out_file,
                    int *out_index, struct io_comp_line **io_resp,
                    int response_count);
int get_io_lines(struct testCase *test, struct io_comp_line **io_lines);
int filter_io_lines(struct io_comp_line **io_lines, int line_count,
                    struct io_comp_line **out_io_lines,
                    enum io_line_type filter);



int main(int argc, char **argv)
{
  struct testCase test_case = {
      OrdIO, "test/10/io", "./a2", {"./a2", "E", NULL}, 0, 0, 0, 0.0f, 10.0f};

  if(argc > 1){
    strcpy(test_case.test_file, argv[1]);
  }

  int test_res = test_runner(&test_case, out_first, &out_first_ptr);
  return 0;
}

int comp(const char *str1, const char *str2, int buff_len)
{
  for (int i = 0; i < buff_len && str1[i] != '\0' && str2[i] != '\0'; i++)
  {
    if (str1[i] != str2[i])
      return 0;
  }
  return 1;
}

int strsrch(const char *str1, const char *str2, int buff_len)
{
  int i = 0;
  int len = -1;
  int pos = -1;
  for (i = 0; i < buff_len && str1[i] != 0; i++)
    ;
  if (i < buff_len)
    len = i;
  else
    return -1;
  do
  {
    for (i = pos + 1; i < len && str1[i] != str2[0]; i++)
      ;
    if (i == len)
      return -1;
    pos = i;
    for (i = 0; pos + i < len && str1[pos + i] == str2[i] && str2[i] != 0; i++)
      ;
    if (str2[i] == 0)
      return pos;
  } while (pos >= 0);
  return -1;
}

int test_runner(struct testCase *test, char *out_file, int *out_index)
{
  struct io_comp_line **io_lines = NULL;
  struct io_comp_line **io_resps = NULL;
  int line_count = get_io_lines(test, io_lines);
  if (line_count == -1)
  {
    free(io_lines);
    return 3;
  }
  int resp_count = filter_io_lines(io_lines, line_count, io_resps, line_in);
  for (int i = 0; i < resp_count; i++)
    printf("io %2d: %s", i, io_resps[i]->line);

  int f_stdin[2];
  int f_stdout[2];

  if (pipe(f_stdin) == -1)
  {
    perror("Failed to open stdin pipe\n");

    free(io_lines);
    return 2;
  }

  if (pipe(f_stdout) == -1)
  {
    perror("Failed to open stdout pipe\n");
    close(f_stdin[0]);
    close(f_stdin[1]);

    free(io_lines);
    return 2;
  }

  pid_t pid = fork();
  printf("Process %d uses in pipe %d -> %d\n", getpid(), f_stdin[0], f_stdin[1]);
  printf("Process %d uses outpipe %d -> %d\n", getpid(), f_stdout[0], f_stdout[1]);
  if (pid == -1)
  {
    perror("fork failed");
    free(io_lines);
    return 1;
  }
  else if (pid == 0)
  {
    setbuf(stdout, NULL);
    close(f_stdout[0]);
    close(f_stdin[1]);

    dup2(f_stdout[1], 1);
    dup2(f_stdin[0], 0);

    close(f_stdout[1]);
    close(f_stdin[0]);
    free(io_lines);
    execvp(test->prog_file, test->args);
    perror("exec failed");
    return -2;
  }

  // set up the signal handlers after forking so the child doesn't inherit them

  signal(SIGALRM, alarm_handler);
  signal(SIGCHLD, child_handler);

  sleep(1);

  alarm(TIME_LIMIT); // install an alarm to be fired after TIME_LIMIT

  int ret_resp = io_pipe_handler(f_stdin, f_stdout, out_file, out_index,
                                 io_lines, resp_count);
  if (ret_resp != 0)
  {
    perror("response handler failed\n");
    free(io_lines);
    return ret_resp;
  }
  if (timeout == 0)
    pause();

  int wstatus;
  int result = waitpid(pid, &wstatus, WNOHANG);
  if (timeout && result == 0)
  {
    printf("alarm triggered\n");

    // child still running, so kill it
    printf("killing child\n");
    kill(pid, 9);
    wait(NULL);
    free(io_lines);
    return -1;
  }
  else if (test_done)
  {
    printf("Wstatus %d\n", wstatus);
    if (WIFEXITED(wstatus))
    {
      int status = WEXITSTATUS(wstatus);
      printf("Process %d exited with Code %d\n", result, status);
      test->ret_val_got = status;
    }
  }
  free(io_lines);
  return 0;
}

void child_handler(int sig)
{
  test_done = 1;
  sig += 0;
}

void alarm_handler(int sig)
{
  timeout = 1;
  sig += 0;
}

int io_pipe_handler(int f_stdin[2], int f_stdout[2], char *out_file,
                    int *out_index, struct io_comp_line **io_resp,
                    int response_count)
{
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

  while ((ret = poll(fds, 2, 100)))
  {
    if (!ret)
    {
      printf("%d miliseconds elapsed.\n", 100);
    }
    if (timeout)
    {
      close(f_stdout[0]);
      close(f_stdin[1]);
      perror("Timeout occured");
      return -1;
    }

    if (fds[1].revents & POLLOUT)
    {
      if (respNr == response_count)
      {
        close(f_stdin[1]);
        break;
      }
      printf("%d < %s", f_stdin[1], io_resp[respNr]->line);
      write(f_stdin[1], io_resp[respNr]->line, io_resp[respNr]->length);
      respNr++;
    }
  }

  while ((readLen = read(f_stdout[0], buff, sizeof(buff))) > 0)
  {
    write(1, buff, readLen);
    fflush(stdout);
    int i = 0;
    for (; i < LINE_BUFF_SIZE && i < readLen && buff[i] != 0; i++)
    {
      out_file[*out_index++] = buff[i];
      buff[i] = 0;
    }
    for (; i < LINE_BUFF_SIZE; i++)
      buff[i] = 0;
  }

  close(f_stdout[0]);
  close(f_stdin[1]);
  return 0;
}

int get_io_lines(struct testCase *test, struct io_comp_line **io_lines)
{
  FILE *fptr = fopen(test->test_file, "r");

  io_lines = (struct io_comp_line **)(malloc(SIZE_IO_MAX_PTR));
  if(io_lines == NULL)
    return -1;
  for (int i = 0; i < IO_MAX_LINES; i++)
    io_lines[i] = malloc(SIZE_IO_LINE);

  int lines = 0;
  char buff[LINE_BUFF_SIZE];
  while (fgets(buff, LINE_BUFF_SIZE, fptr) != NULL)
  {
    if (buff[0] == '>')
      io_lines[lines]->type = line_out;
    else if (buff[0] == '<')
      io_lines[lines]->type = line_in;
    else if (buff[0] == '?')
      io_lines[lines]->type = line_req;
    else if (buff[0] == '!')
      break;
    else
    {
      fclose(fptr);
      return -1;
    }
    int i = 0;
    for (;
         i + 2 < LINE_BUFF_SIZE && buff[i + 2] != '\n' && buff[i + 2] != '\0';
         i++)
    {
      io_lines[lines]->line[i] = buff[i + 2];
    }
    io_lines[lines]->line[i] = '\n';
    io_lines[lines]->length = i + 1;
    lines++;
  }
  fclose(fptr);
  return lines;
}

int filter_io_lines(struct io_comp_line **io_lines, int line_count, 
                    struct io_comp_line **out_io_lines, 
                    enum io_line_type filter)
{
  out_io_lines = (struct io_comp_line **)(malloc(SIZE_IO_MAX_PTR));
  if(out_io_lines == NULL)
    return -1;
  for (int i = 0; i < IO_MAX_LINES; i++)
    out_io_lines[i] = (struct io_comp_line *)(malloc(SIZE_IO_LINE));

  int lines = 0;
  for (int i = 0; i < line_count; i++)
  {
    if (io_lines[i]->type == filter)
    {
      int j = 0;
      for (; j < LINE_BUFF_SIZE && io_lines[i]->line[j] != '\n' && io_lines[i]->line[j] != '\0'; j++)
      {
        out_io_lines[lines]->line[j] = io_lines[lines]->line[j];
      }
      out_io_lines[lines]->line[j] = '\n';
      out_io_lines[lines]->length = j + 1;
      lines++;
    }
  }
  return lines;
}
