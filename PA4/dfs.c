#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <netdb.h>
#include <dirent.h>
#include <pthread.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAXLINE 8192
#define BUF 256
#define LISTENQ 1024

char path[6]; // stores local path, e.g "/DFS1"

int open_listenfd(int port);
void process(int connfd);
void* thread(void* vargp);
void createUserDir(char* user);
bool auth(char* msg, char* userBuf);

int main(int argc, char** argv){
  int listenfd, *connfdp, portno, clientlen=sizeof(struct sockaddr_in);
  struct sockaddr_in clientaddr;
  pthread_t tid;

  if(argc != 3){
    printf("Invalid number of arguments - usage: ./dfs </DFS{1-4}> <port #>\n;");
    return 1;
  }

  if(strcmp(argv[1], "/DFS1") && strcmp(argv[1], "/DFS2") && strcmp(argv[1], "/DFS3") && strcmp(argv[1], "/DFS4")){
    printf("Invalid DFS - usage: ./dfs </DFS{1-4}> <port #>\n");
    return 1;
  }
  portno = atoi(argv[2]);
  strcpy(path, argv[1]);

  listenfd = open_listenfd(portno);

  while(1){
    printf("Waiting for connection...\n");
    connfdp = malloc(sizeof(int));
    *connfdp = accept(listenfd, (struct sockaddr*)&clientaddr, &clientlen);
    pthread_create(&tid, NULL, thread, connfdp);
  }
}

void* thread(void* vargp){
  int connfd = *((int*)vargp);
  pthread_detach(pthread_self());
  free(vargp);
  process(connfd);
  close(connfd);
  return NULL;
}

void process(int connfd){
  char user[BUF];
  while(1){
    size_t n;
    char buf[MAXLINE];
    bzero(buf, MAXLINE);
    n = read(connfd, buf, MAXLINE);
    if(n == 0){
      break;
    }
    printf("Received:\n%s\n", buf);

    if(!strncmp(buf, "auth", 4)){
      if(auth(buf, user)){
        write(connfd, "auth", 4);
      } else {
        write(connfd, "noauth", 6);
      }
    }

    else if(!strncmp(buf, "get", 3)){
      struct stat s;
      int fsize;
      char* fname = strchr(buf, ' ')+1;
      char filepath[strlen(fname) + strlen(path) + strlen(user) + 4];
      strcpy(filepath, ".");
      strcat(filepath, path);
      strcat(filepath, "/");
      strcat(filepath, user);
      strcat(filepath, "/");
      strcat(filepath, fname);

      createUserDir(user);
      printf("Checking for existance of file %s\n", filepath);
      if(stat(filepath, &s) == 0){
        fsize = (int)s.st_size;
        if(fsize == 0){
          write(connfd, "dne", 3);
          continue;
        }
        printf("File size: %d\n", fsize);
        char str[32];
        sprintf(str, "%d", fsize);
        write(connfd, str, strlen(str));
      } else {
        write(connfd, "dne", 3);
        continue;
      }

      char chunk[fsize];
      memset(chunk, 0, fsize);
      FILE* f = fopen(filepath, "rb");
      if(!f){
        printf("Error opening file###\n");
      }
      fread(chunk, fsize, 1, f);
      fclose(f);

      char ready_buf[5];
      read(connfd, ready_buf, 5);
      printf("Read file: \n%s\n", chunk);
      write(connfd, chunk, fsize);
    }

    else if(!strncmp(buf, "put", 3)){
      createUserDir(user);
      char* fname = strchr(buf, ' ')+1;
      *(fname - 1) = '\0';
      char* fsize = strchr(buf, ':')+1;
      int fileSize = atoi(fsize);
      printf("File size is %d\n", fileSize);
      printf("Got request put %s\n", fname);
      char dir[strlen(path) +strlen(fname) + strlen(user) +  5]; //4 more bytes for slash before filename and user, dot prefix, and null terminator
      bzero(dir, sizeof(dir));
      strcpy(dir, ".");
      strcat(dir, path);
      strcat(dir, "/");
      strcat(dir, user);
      strcat(dir, "/");
      strcat(dir, fname);

      printf("Final dir is %s\n", dir);
      FILE* f, *tst;
      f = fopen(dir, "wb");
      if(!f){
        printf("Error opening file\n");
        continue;
      }

      printf("1\n");
      char filebuf[fileSize];
      memset(filebuf, 0, sizeof(filebuf));
      printf("2\n");
      n = read(connfd, filebuf, fileSize);
      printf("3\n");
      printf("Received %d bytes:\n", (int)n);
      if(n < 0){
        printf("Error in put:read\n");
        break;
      }
      printf("4\n");
      fwrite(filebuf, 1, n, f);
      memset(filebuf, 0, sizeof(filebuf));

      fclose(f);
    }

    else if(!strncmp(buf, "ls", 2)){
      createUserDir(user);

      memset(buf, 0, sizeof(buf));
      char fpath[strlen(path) + strlen(user) + 3];
      memset(fpath, 0, sizeof(fpath));

      strcpy(fpath, ".");
      strcat(fpath, path);
      strcat(fpath, "/");
      strcat(fpath, user);

      printf("Opening dir %s\n", fpath);
      DIR* d = opendir(fpath);
      while(true){
        struct dirent* dir = readdir(d);
        if(dir == NULL){
          break;
        }
        if(strcmp(dir->d_name, ".") && strcmp(dir->d_name, "..")){ //Ignore "." and ".."
          strcat(buf, dir->d_name);
          strcat(buf, "\n");
        }
      }
      closedir(d);
      if(strlen(buf) == 0){
        write(connfd, "nf", 2);
      } else {
        write(connfd, buf, strlen(buf));
      }
    }

    else {
      printf("Command not recognized\n");
    }
  }
}

//! Parses auth message msg, and stores username in user for future use
bool auth(char* msg, char* userBuf){
  char* line = NULL;
  FILE* conf;
  ssize_t len = 0;
  ssize_t bytesRead;

  char* user = strchr(msg, ' ') + 1;
  char* pass = strchr(msg, ':') + 1;
  *(pass-1) = '\0';

  strncpy(userBuf, user, BUF);
  printf("Checking dfc.conf for user:%s pass:%s\n", user, pass);

  conf = fopen("dfs.conf", "r");
  if(conf == NULL){
    printf("Cannot open dfs.conf\n");
    return false;
  }

  while((bytesRead = getline(&line, &len, conf)) != -1){
    line[strcspn(line, "\n")] = 0; //Remove trailing '\n' if it exists
    char* confPass = strchr(line, ' ') + 1;
    *(confPass-1) = '\0';
    if(!strcmp(pass, confPass) && !strcmp(user, line)){
      printf("Match found, successfully authenticated user %s\n", line);
      fclose(conf);
      return true;
    }
  }
  printf("Match not found, rejecting user %s\n", user);
  fclose(conf);
  return false;
}

void createUserDir(char* user){
  DIR* d;
  char dirpath[strlen(path) + strlen(user) + 3]; //3 bytes for preceding '.', slash before user, and null terminator

  strcpy(dirpath, ".");
  strcat(dirpath, path);
  strcat(dirpath, "/");
  strcat(dirpath, user);

  printf("Checking existance of %s\n", dirpath);

  d = opendir(dirpath);
  if(d){
    printf("Directory exists\n");
  } else if(errno == ENOENT){
    printf("Directory does not exist, creating...\n");
    if(mkdir(dirpath, 0777) != 0){
      printf("Error in mkdir: errno %d\n", errno);
    }
  } else {
    printf("Opendir failed\n");
  }
}

/*
 * open_listenfd - open and return a listening socket on port
 * Returns -1 in case of failure
 */
int open_listenfd(int port){
  int listenfd, optval=1;
  struct sockaddr_in serveraddr;

  /* Create a socket descriptor */
  if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    return -1;

  /* Eliminates "Address already in use" error from bind. */
  if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval , sizeof(int)) < 0)
    return -1;

  /* listenfd will be an endpoint for all requests to port on any IP address for this host */
  bzero((char *) &serveraddr, sizeof(serveraddr));
  serveraddr.sin_family = AF_INET;
  serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
  serveraddr.sin_port = htons((unsigned short)port);
  if (bind(listenfd, (struct sockaddr*)&serveraddr, sizeof(serveraddr)) < 0)
    return -1;

  /* Make it a listening socket ready to accept connection requests */
  if (listen(listenfd, LISTENQ) < 0)
    return -1;
  return listenfd;
} /* end open_listenfd */
