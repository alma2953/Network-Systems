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
  printf("Terminating thread\n");
  return NULL;
}

void process(int connfd){
  while(1){
    size_t n;
    char buf[MAXLINE];
    bzero(buf, MAXLINE);
    n = read(connfd, buf, MAXLINE);
    if(n == 0){
      break;
    }
    printf("Received:\n%s\n", buf);
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
