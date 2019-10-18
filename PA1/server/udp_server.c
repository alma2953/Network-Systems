/*
 * udpserver.c - A simple UDP echo server
 * usage: udpserver <port>
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <netdb.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define BUFSIZE 1024

/*
 * error - wrapper for perror
 */
void error(char *msg) {
  perror(msg);
  exit(1);
}

int main(int argc, char **argv) {
  int sockfd; /* socket */
  int portno; /* port to listen on */
  int clientlen; /* byte size of client's address */
  struct sockaddr_in serveraddr; /* server's addr */
  struct sockaddr_in clientaddr; /* client addr */
  struct hostent *hostp; /* client host info */
  char buf[BUFSIZE]; /* message buf */
  char cmd[BUFSIZE];
  char fname[BUFSIZE];
  char *hostaddrp; /* dotted decimal host addr string */
  int optval; /* flag value for setsockopt */
  int n; /* message byte size */

  /*
   * check command line arguments
   */
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }
  portno = atoi(argv[1]);
  if(portno < 5001 || portno > 65534){
      error("Port must be in range 5001-65534");
  }

  /*
   * socket: create the parent socket
   */
  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0)
    error("ERROR opening socket");

  /* setsockopt: Handy debugging trick that lets
   * us rerun the server immediately after we kill it;
   * otherwise we have to wait about 20 secs.
   * Eliminates "ERROR on binding: Address already in use" error.
   */
  optval = 1;
  setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,
	     (const void *)&optval , sizeof(int));

  /*
   * build the server's Internet address
   */
  bzero((char *) &serveraddr, sizeof(serveraddr));
  serveraddr.sin_family = AF_INET;
  serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
  serveraddr.sin_port = htons((unsigned short)portno);

  /*
   * bind: associate the parent socket with a port
   */
  if (bind(sockfd, (struct sockaddr *) &serveraddr,
	   sizeof(serveraddr)) < 0)
    error("ERROR on binding");

  /*
   * main loop: wait for a datagram, then echo it
   */
  clientlen = sizeof(clientaddr);
  while (1) {

    /*
     * recvfrom: receive a UDP datagram from a client
     */
    bzero(buf, BUFSIZE);
    n = recvfrom(sockfd, buf, BUFSIZE, 0,
		 (struct sockaddr *) &clientaddr, &clientlen);
    if (n < 0)
      error("ERROR in recvfrom");

    /*
     * gethostbyaddr: determine who sent the datagram
     */
    hostp = gethostbyaddr((const char *)&clientaddr.sin_addr.s_addr,
			  sizeof(clientaddr.sin_addr.s_addr), AF_INET);
    if (hostp == NULL)
      error("ERROR on gethostbyaddr");
    hostaddrp = inet_ntoa(clientaddr.sin_addr);
    if (hostaddrp == NULL)
      error("ERROR on inet_ntoa\n");
    printf("server received datagram from %s (%s)\n",
	   hostp->h_name, hostaddrp);
    printf("server received %d/%d bytes: %s\n", (int)strlen(buf), n, buf);


    sscanf(buf, "%s %s", cmd, fname);
    if(!strncmp(cmd, "get", 3)){
        bzero(buf, sizeof(buf));
        clientlen = sizeof(clientaddr);
        printf("Retrieving file %s\n", fname);
        FILE* f = fopen(fname, "rb");
        if(f == NULL){
            strncpy(buf, "dne", 3);
            printf("Error: Requested file %s does not exist in directory.\n", fname);
            n = sendto(sockfd, buf, strlen(buf), 0,
                   (struct sockaddr *) &clientaddr, clientlen);
            if (n < 0)
              error("ERROR in sendto");
            continue;
        }
        //Calculate file size to tell the client how many bytes to expect
        fseek(f, 0L, SEEK_END);
        int file_size = ftell(f);
        rewind(f);
        sprintf(buf, "%d", file_size);
        printf("Size of file: %s Bytes - sending size to server\n", buf);
        n = sendto(sockfd, buf, strlen(buf), 0,
               (struct sockaddr *) &clientaddr, clientlen);
        if (n < 0)
          error("ERROR in sendto");
        bzero(buf, sizeof(buf));

        //Send file to client 1024 bytes at a time
        while(fread(buf, 1, sizeof(buf), f) != 0){
            n = sendto(sockfd, buf, sizeof(buf), 0,
                   (struct sockaddr *) &clientaddr, clientlen);
            if (n < 0)
              error("ERROR in sendto");
            bzero(buf, sizeof(buf));
        }
        fclose(f);
    }
    else if(!strncmp(cmd, "put", 3)){
        printf("Receiving file %s from client.\n", fname);
        n = recvfrom(sockfd, buf, BUFSIZE, 0, (struct sockaddr *)&clientaddr, &clientlen);
        if (n < 0)
          error("ERROR in recvfrom");

        printf("Filesize: %s\n", buf);
        int file_size = atoi(buf);
        FILE* f = fopen(fname,"wb");
        if(f == NULL){
            error("Error opening put filename");
        }

        int bytes_transfered = 0;
        while(true){
            bzero(buf, sizeof(buf));
            int transfer_size = BUFSIZE;
            if(file_size - bytes_transfered < 1024){
                //Less than 1024 bytes remaining, don't send full buffer
                transfer_size = file_size - bytes_transfered;
            }
            n = recvfrom(sockfd, buf, transfer_size, 0, (struct sockaddr *)&clientaddr, &clientlen);
            if (n < 0)
              error("ERROR in recvfrom");
            bytes_transfered += transfer_size;
            printf("Received %d of %d bytes\n", bytes_transfered, file_size);
            fwrite(buf, transfer_size, 1, f);
            if (bytes_transfered >= file_size){
                printf("File transferred successfully.\n");
                fclose(f);
                break;
            }
        }
    }
    else if(!strncmp(cmd, "delete", 6)){
        printf("Deleting file %s\n", fname);
        bzero(buf, sizeof(buf));
        FILE* f = fopen(fname, "rb");
        if (f == NULL){
            //Tell client the file doesn't exist
            strncpy(buf, "dne", 3);
            printf("Error: Requested file %s does not exist in directory.\n", fname);
            n = sendto(sockfd, buf, strlen(buf), 0,
                   (struct sockaddr *) &clientaddr, clientlen);
            if (n < 0)
              error("ERROR in sendto");
            continue;
        }
        fclose(f);
        n = remove(fname);
        if (n < 0)
          error("Error in remove");
        printf("Successfully deleted file %s.\n", fname);
        strcpy(buf, "Deleted file");
        n = sendto(sockfd, buf, strlen(buf), 0,
               (struct sockaddr *) &clientaddr, clientlen);
        if (n < 0)
          error("ERROR in sendto");
    }
    else if(!strncmp(cmd, "ls", 2)){
        bzero(buf, sizeof(buf));
        DIR* d = opendir(".");
        while(true){
            //Loop through each file in dir and add its name to the buffer
            struct dirent* dir = readdir(d);
            if(dir == NULL){
                break;
            }
            strcat(buf, dir->d_name);
            strncat(buf, "\n", 1);
        }
        n = sendto(sockfd, buf, strlen(buf), 0,
               (struct sockaddr *) &clientaddr, clientlen);
        if (n < 0)
          error("ERROR in sendto");
    }
    else if(!strncmp(cmd, "exit", 4)){
        printf("Recieved exit signal, exiting program.\n");
        break;
    } else {
        printf("Unknown command %s, echoing back.\n", cmd);
        bzero(buf, strlen(buf));
        sprintf(buf, "Command %s not understood by server.\n", cmd);
        n = sendto(sockfd, buf, strlen(buf), 0,
               (struct sockaddr *) &clientaddr, clientlen);
        if (n < 0)
          error("ERROR in sendto");
    }
  }
  return 0;
}
