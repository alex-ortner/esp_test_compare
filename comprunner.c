#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/poll.h>

int child();
int parent(int, int[2], int[2]);
int comp(const char* str1, const char* str2, int buff_len);
int strsrch(const char* str1, const char* str2, int buff_len);
char* responses[]={
  "CODE\n",
  "EESI\n",
  "REDN\n",
  "ENO\n",
  "YARI\n"
};
int len_responses[] = {5,5,5,4,5};
int main(int argc, char** argv)
{
  int retValue = 0;
  int fd[2];
  int fd2[2];
  if(pipe(fd) == -1)
    printf("Error occured while opening pipe");

  if(pipe(fd2) == -1)
    printf("Error occured while opening pipe");

  if(argc > 1){
    printf("Got Arguments:\n");
    for(int i = 0; i < argc; i++){
      printf("  %s\n", argv[i]);
    }
    fflush(stdout);
  }
  printf("Process id(%d), pipe %d : %d\n", getpid(), fd[0], fd[1]);
  printf("Process id(%d), pipe %d : %d\n", getpid(), fd2[0], fd2[1]);

  int id = fork();

  if(id == 0)
  {
    setbuf(stdout, NULL);
    close(fd[0]);
    close(fd2[1]);

    dup2(fd[1], 1);
    dup2(fd2[0], 0);

    close(fd[1]);
    close(fd2[0]);

    retValue = child();
  }
  else
  {
    retValue = parent(id, fd, fd2);
    fflush(stdout);
    close(fd[0]);
    int wstatus;
    int res = wait(&wstatus);
    printf("Wstatus %d\n", wstatus);
    if(WIFEXITED(wstatus)){
      int status = WEXITSTATUS(wstatus);
      printf("Process %d exited with Code %d\n", res, status);
    }
  }



  return 0;
}

int child()
{
  char* args[] = {
    "./a2",
    "A",
    NULL
  };
  return execv("./a2",  args);
}

int parent(int id, int fd[2], int fd2[2]){
  char buff[256];
  int respNr = 0;
  int respMax = 5;
  int readLen = 0;
  close(fd[1]);
  close(fd2[0]);
  printf("From Child\n\n");


	struct pollfd fds[2];
	int ret;

	/* watch stdin for input */
	fds[0].fd = fd[0];
	fds[0].events = POLLIN;

	/* watch stdout for ability to write */
	fds[1].fd = fd2[1];
	fds[1].events = POLLOUT;

  while((ret = poll(fds, 2, 100))){
	  if (!ret) {
		  printf ("%d miliseconds elapsed.\n", 500);
	  }

	  if (fds[1].revents & POLLOUT){
      if(respNr == respMax)
      {
        close(fd2[1]);
        break;
      }
      write(fd2[1], responses[respNr], len_responses[respNr++]);
    }
  }

  while((readLen = read(fd[0], buff, sizeof(buff))) > 0)
  {
    write(1, buff, readLen);
    fflush(stdout);
    for(int i = 0; i < 256; i++) buff[i] = 0;

  }

  printf("\n\nParent Error (%d): %d\n", id, errno);
  close(fd[0]);
  close(fd2[1]);
  return 0;
}

int comp(const char* str1, const char* str2, int buff_len){
  for(int i = 0; i < buff_len && str1[i] != '\0' && str2[i] != '\0'; i++){
    if(str1[i] != str2[i])
      return 0;
  }
  return 1;
}

int strsrch(const char* str1, const char* str2, int buff_len){
  int i = 0;
  int len = -1;
  int pos = -1;
  for(i = 0; i < buff_len && str1[i] != 0; i++);
  if(i < buff_len) len = i;
  else return -1;
  do{
    for(i = pos + 1; i < len && str1[i] != str2[0]; i++);
    if( i == len) return -1;
    pos = i;
    for(i = 0; pos + i < len && str1[pos + i] == str2[i] && str2[i] != 0; i++);
    if( str2[i] == 0) return pos;
  }while(pos >= 0);
  return -1;
}
