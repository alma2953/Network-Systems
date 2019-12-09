#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <netdb.h>
#include <dirent.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <openssl/md5.h>

#define MAXLINE 8192
#define BUF 256

#define DFS1 0
#define DFS2 1
#define DFS3 2
#define DFS4 3

int sockfd[4];
struct sockaddr_in serveraddr[4];

void initServerDirs();
void createSocket(int dfsIdx, int port);
int sendtoDFS(char* msg, int dfsIdx);
int hashBucket(char* fname);

int main(int argc, char** argv){
  int port1, port2, port3, port4, n;
  char user[BUF];
  char pass[BUF];
  char userInput[BUF];
  char cmd[BUF];
  char fname[BUF];
  FILE* conf;
  char* line = NULL;
  ssize_t len = 0;

  if(argc != 2){
    printf("Usage: ./dfc <config_file_name.conf>\n");
    return 1;
  }

  initServerDirs();

  conf = fopen(argv[1], "r");
  if(conf == NULL){
    printf("Cannot open config file %s\n", argv[1]);
    return 1;
  }

  for(int i = 0; i<6; i++){ //Read through the 6 lines of the conf file
    getline(&line, &len, conf);
    line[strcspn(line, "\n")] = 0; //Remove trailing '\n' if it exists
    if(i == 0){ //DFS1
      port1 = atoi(strchr(line, ':') + 1);
    } else if(i == 1){ //DFS2
      port2 = atoi(strchr(line, ':') + 1);
    } else if(i == 2){ //DFS3
      port3 = atoi(strchr(line, ':') + 1);
    } else if(i == 3){ //DFS4
      port4 = atoi(strchr(line, ':') + 1);
    } else if(i == 4){ //Username
      strncpy(user, strchr(line, ' ') + 1, BUF);
    } else { //Password
      strncpy(pass, strchr(line, ' ') + 1, BUF);
    }
  }
  fclose(conf);
  printf("Parsed file %s:\nDFS port 1: %d\nDFS port 2: %d\nDFS port 3: %d\nDFS port 4: %d\nUser: %s\nPass: %s\n", argv[1], port1, port2, port3, port4, user, pass);

  createSocket(DFS1, port1);
  createSocket(DFS2, port2);
  createSocket(DFS3, port3);
  createSocket(DFS4, port4);

  while(1){ //Program loop
    bzero(userInput, BUF);
    printf("\nEnter one of the following commands:\n * get [filename]\n * put [filename]\n * ls\n> ");
    fgets(userInput, BUF, stdin);
    sscanf(userInput, "%s %s", cmd, fname);

    if(!strcmp(cmd, "get")){
      struct stat s;
      if(stat(fname, &s) != 0){
        printf("File %s does not exist.\n", fname);
        continue;
      }
      int bucket = hashBucket(fname);
      printf("Hash mod 4 is: %d\n", bucket);
    } else if (!strcmp(cmd, "put")){

    } else if (!strcmp(cmd, "ls")){

    } else {
      printf("Invalid command.\n");
    }
  }

  return 0;
}

//! Creates server directories if they don't exist already
void initServerDirs(){
  struct stat s = {0};
  if(stat("./DFS1", &s) == -1){
    mkdir("./DFS1", 0777);
  }
  if(stat("./DFS2", &s) == -1){
    mkdir("./DFS2", 0777);
  }
  if(stat("./DFS3", &s) == -1){
    mkdir("./DFS3", 0777);
  }
  if(stat("./DFS4", &s) == -1){
    mkdir("./DFS4", 0777);
  }
}

//! Initializes socket for DFS<dfsIdx+1> on port# <port>
void createSocket(int dfsIdx, int port){
  struct hostent* server;

  sockfd[dfsIdx] = socket(AF_INET, SOCK_STREAM, 0);
  if(sockfd[dfsIdx] < 0){
    printf("Error creating socket\n");
    exit(1);
  }

  server = gethostbyname("127.0.0.1");
  if(server == NULL){
    printf("Error resolving hostname\n");
    exit(1);
  }

  //struct sockaddr_in sock = serveraddr[dfsIdx];
  bzero((char *) &serveraddr[dfsIdx], sizeof(struct sockaddr_in));
  serveraddr[dfsIdx].sin_family = AF_INET;
  bcopy((char *)server->h_addr,
  (char *)&serveraddr[dfsIdx].sin_addr.s_addr, server->h_length);
  serveraddr[dfsIdx].sin_port = htons(port);

  if(connect(sockfd[dfsIdx], (struct sockaddr*)&serveraddr[dfsIdx], sizeof(struct sockaddr_in)) != 0){
    printf("Error in connect: errno %d\n", errno);
    exit(1);
  }
}

//! Sends msg to specified DFS
int sendtoDFS(char* msg, int dfsIdx){
  int n;

  n = write(sockfd[dfsIdx], msg, strlen(msg));
  if(n < 0){
    printf("Error in sendto: errno %d\n", errno);
    exit(1);
  }
  return n;
}

//! Returns md5sum() % 4 of file fname, implementation from https://stackoverflow.com/questions/10324611/how-to-calculate-the-md5-hash-of-a-large-file-in-c
int hashBucket(char* fname){
  unsigned char c[MD5_DIGEST_LENGTH];
  FILE* f = fopen(fname, "rb");
  MD5_CTX context;
  int bytes, modResult = 0;
  unsigned char data[MAXLINE];

  MD5_Init(&context);
  while((bytes = fread(data, 1, MAXLINE, f)) != 0){
    MD5_Update(&context, data, bytes);
  }
  MD5_Final(c, &context);

  for(int i = 0; i < MD5_DIGEST_LENGTH; i++) {
    int md5Char = (int)c[i];
    modResult = (modResult*16 + md5Char) % 4;
  }
  
  fclose (f);
  return modResult;
}
