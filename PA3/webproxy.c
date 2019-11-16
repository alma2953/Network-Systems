#include <stdio.h>
#include <stdlib.h>
#include <string.h>      /* for fgets */
#include <strings.h>     /* for bzero, bcopy */
#include <unistd.h>      /* for read, write */
#include <sys/socket.h>  /* for socket use */
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/time.h>

#define MAXLINE  8192  /* max text line length */
#define MAXBUF   8192  /* max I/O buffer size */
#define LISTENQ  1024  /* second argument to listen() */

int open_listenfd(int port);
void echo(int connfd);
void *thread(void *vargp);

int main(int argc, char **argv)
{
    int listenfd, *connfdp, port, clientlen=sizeof(struct sockaddr_in);
    struct sockaddr_in clientaddr;
    struct timeval timeout;
    pthread_t tid;

    if (argc == 2){
      timeout.tv_sec = 60;
      timeout.tv_usec = 0;
    } else if (argc == 3){
      timeout.tv_sec = atoi(argv[2]);
      timeout.tv_usec = 0;
    } else {
	fprintf(stderr, "usage: %s <port>\nor: %s <port> <timeout>\n", argv[0], argv[0]);
	exit(0);
    }
    port = atoi(argv[1]);

    listenfd = open_listenfd(port);
    while (1) {
	connfdp = malloc(sizeof(int));
	*connfdp = accept(listenfd, (struct sockaddr*)&clientaddr, &clientlen);
	pthread_create(&tid, NULL, thread, connfdp);
    }
}

/* thread routine */
void * thread(void * vargp)
{
    int connfd = *((int *)vargp);
    pthread_detach(pthread_self());
    free(vargp);
    echo(connfd);
    close(connfd);
    return NULL;
}

/* Sends 500 error message back to server*/

//500 Internal Server Error
void send_error(int connfd, char* msg)
{
    char errormsg[MAXLINE];
    sprintf(errormsg, "HTTP/1.1 %s\r\nContent-Type:text/plain\r\nContent-Length:0\r\n\r\n", msg);
    write(connfd, errormsg, strlen(errormsg));
}

/* Puts appropriate Content-Type field in buf based on ext */
int get_content_type(char* buf, char* ext)
{
  if(!strcmp(ext, "html")){
    strcpy(buf, "text/html");
  } else if (!strcmp(ext, "txt")){
    strcpy(buf, "text/plain");
  } else if (!strcmp(ext, "png")){
    strcpy(buf, "image/png");
  } else if (!strcmp(ext, "gif")){
    strcpy(buf, "image/gif");
  } else if (!strcmp(ext, "jpg")){
    strcpy(buf, "image/jpg");
  } else if (!strcmp(ext, "css")){
    strcpy(buf, "text/css");
  } else if (!strcmp(ext, "js")){
    strcpy(buf, "application/javascript");
  } else {
    return -1;
  }
  return 0;
}

void get_req(int connfd, char* fname, char* version)
{
  FILE* f;
  ssize_t fsize;
  char file_extension[MAXBUF];
  char content_type[MAXBUF]; // String to be placed in content-type field of response
  char relative_dir[MAXBUF]; // Stores filename relative to server directory

  if(!strcmp(fname, "/")){ // Default - get homepage
    printf("Default page requested\n");
    f = fopen("www/index.html", "rb");

    // Calculate file size to put in header
    fseek(f, 0L, SEEK_END);
    fsize = ftell(f);
    rewind(f);
    printf("Filesize: %d\n", (int)fsize);

    strcpy(file_extension, "html");
  } else { // Get specified file
    strcat(relative_dir, "www");
    strcat(relative_dir, fname);
    printf("File requested: %s\n", fname+1);
    f = fopen(relative_dir, "rb");

    if(f == NULL){
      printf("Cannot open file %s\n", fname+1);
      send_error(connfd, "500 Internal Server Error");
      return;
    }

    // Calculate file size to put in header
    fseek(f, 0L, SEEK_END);
    fsize = ftell(f);
    rewind(f);
    printf("Filesize: %d\n", (int)fsize);

    char* dot = strchr(fname, '.'); // Find pointer to '.' char in filename, extension starts at the next char
    if(!dot){
      printf("Cannot find extension of file %s\n", fname+1);
      send_error(connfd, "500 Internal Server Error");
      return;
    }
    strncpy(file_extension, dot+1, 4); // Copy up to 4 bytes of extension after '.'
  }

  printf("File extension: %s\n", file_extension);

  if(get_content_type(content_type, file_extension) == -1){ // fills content_type based on extension, returns -1 on error
    printf("Unsupported extension %s\nResponding with error\n", file_extension);
      send_error(connfd, "500 Internal Server Error");
    return;
  }

  printf("Content type: %s\n", content_type);

  char file_contents[fsize];
  fread(file_contents, 1, fsize, f);

  char header[MAXBUF];
  sprintf(header, "%s 200 Document Follows\r\nContent-Type:%s\r\nContent-Length:%ld\r\n\r\n", version, content_type, fsize);

  char http_response[fsize + strlen(header)];
  strcpy(http_response, header);
  memcpy(http_response+strlen(header), file_contents, fsize); // Using strcpy fails on non-text files because of the precense of '\0' bytes

  printf("Sending http response to server: \n\n%s", http_response);
  write(connfd, http_response, fsize+strlen(header));
}


/*
 * echo - read and echo text lines until client closes connection
 */

void echo(int connfd)
{
    size_t n;
    char buf[MAXLINE];
    struct sockaddr_in serveraddr;
    struct hostent* server;
    int sockfd;
    int size;
    int portno = 80;

    n = read(connfd, buf, MAXLINE);
    printf("\nserver received the following request:\n%s\n",buf);

    char* request = strtok(buf, " "); // GET or POST
    char* hostname = strtok(NULL, " ");
    char* version = strtok(NULL, "\r"); // HTTP/1.1 or HTTP/1.0
    char fname[MAXLINE];

    printf("Request type: %s\nHostname: %s\nVersion:%s\n", request, hostname, version);

    if(hostname == NULL || version == NULL){
      printf("Hostname or version is NULL\n");
      send_error(connfd, "500 Internal Server Error");
      return;
    }

    if(strlen(hostname) == 0){
      printf("No host requested, responding with error\n");
      send_error(connfd, "500 Internal Server Error");
      return;
    }

    if(!strcmp(version, "HTTP/1.1") || !strcmp(version, "HTTP/1.0")){
    } else {
      printf("Invalid HTTP version: %s\nResponding with error\n", version);
      send_error(connfd, "500 Internal Server Error");
      return;
    }

    if(!strcmp(request, "GET")){
      //get_req(connfd, hostname, version);
    } else {
      printf("Invalid HTTP request: %s\nResponding with 400 error\n", request);
      send_error(connfd, "400 Bad Request");
      return;
    }

    char* slash = strrchr(hostname, '/');
    if(*(slash+1) == '\0'){ //If no file is explicitly requested, get default
      printf("Default page requested\n");
      strcpy(fname, "index.html");
    } else { //Otherwise, copy requested filename to buffer
      strcpy(fname, slash+1);
      printf("Host: %s\nFile: %s\n", hostname, fname);
    }

    // Set the slash to null terminator as it will fail gethostbyname otherwise
    // e.g google.com/ will fail, while google.com will work
    *slash = '\0';

    slash = strrchr(hostname, '/'); //This will point to the last / in http://

    // Set hostname to start after, as it will fail gethostbyname otherwise
    // e.g http://google.com will fail, while google.com will work
    hostname = slash+1;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0){
      printf("ERROR opening socket");
    }

    server = gethostbyname(hostname);
    if(server == NULL){
      printf("Unable to resolve host %s, responding with 404 error\n", hostname);
      send_error(connfd, "404 Not Found");
      return;
    }

    printf("Host: %s, IP: %s\n", hostname, server->h_addr);

    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    bcopy((char *)server->h_addr,
    (char *)&serveraddr.sin_addr.s_addr, server->h_length);
    serveraddr.sin_port = htons(portno);

    int serverlen = sizeof(serveraddr);

    size = connect(sockfd, (struct sockaddr*)&serveraddr, serverlen);
    if (size < 0){
      printf("ERROR in connect\n");
      return;
    }


    sprintf(buf, "GET /%s HTTP/1.1", fname);

    printf("sending\n");

    size = write(sockfd, buf, strlen(buf));
    if (size < 0)
        printf("ERROR in sendto\n");
        return;

    printf("sent\n");
    memset(buf, 0, sizeof(buf));
    size = read(sockfd, buf, sizeof(buf));
    if (size < 0)
        printf("ERROR in recvfrom\n");
        return;

    printf("received\n");

    printf("BUFFER RESPONSE ########:\n\n%s\n", buf);
}

/*
 * open_listenfd - open and return a listening socket on port
 * Returns -1 in case of failure
 */
int open_listenfd(int port)
{
    int listenfd, optval=1;
    struct sockaddr_in serveraddr;

    /* Create a socket descriptor */
    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        return -1;

    /* Eliminates "Address already in use" error from bind. */
    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR,
                   (const void *)&optval , sizeof(int)) < 0)
        return -1;

    /* listenfd will be an endpoint for all requests to port
       on any IP address for this host */
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
