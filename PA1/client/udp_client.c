/*
 * udpclient.c - A simple UDP client
 * usage: udpclient <host> <port>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>

#define BUFSIZE 1024

/*
 * error - wrapper for perror
 */
void error(char *msg) {
    perror(msg);
    exit(0);
}

int main(int argc, char **argv) {
    int sockfd, portno, n;
    int serverlen;
    struct sockaddr_in serveraddr;
    struct hostent *server;
    char *hostname;
    char buf[BUFSIZE];
    char cmd[BUFSIZE];
    char fname[BUFSIZE];
    bool put_cmd = false;

    /* check command line arguments */
    if (argc != 3) {
       fprintf(stderr,"usage: %s <hostname> <port>\n", argv[0]);
       exit(0);
    }
    hostname = argv[1];
    portno = atoi(argv[2]);
    if(portno < 5001 || portno > 65534){
        error("Port must be in range 5001-65534");
    }

    /* socket: create the socket */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
        error("ERROR opening socket");

    /* gethostbyname: get the server's DNS entry */
    server = gethostbyname(hostname);
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host as %s\n", hostname);
        exit(0);
    }

    /* build the server's Internet address */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    bcopy((char *)server->h_addr,
	  (char *)&serveraddr.sin_addr.s_addr, server->h_length);
    serveraddr.sin_port = htons(portno);

    while(1){
      /* get a message from the user */
        bzero(buf, BUFSIZE);
        printf("\nEnter one of the following commands:\n * get [filename]\n * put [filename]\n * delete [filename]\n * ls\n * exit\n> ");
        fgets(buf, BUFSIZE, stdin);
        sscanf(buf, "%s %s", cmd, fname);

        if(!strncmp(cmd, "get", 3)){
            serverlen = sizeof(serveraddr);
            n = sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr *)&serveraddr, serverlen);
            if (n < 0)
                error("ERROR in sendto");

            n = recvfrom(sockfd, buf, sizeof(buf), 0, (struct sockaddr *)&serveraddr, &serverlen);
            if (n < 0)
                error("ERROR in recvfrom");

            if(!strncmp(buf, "dne", 3)){ //If server responds that the file doesn't exist
                    printf("The requested file %s does not exist on the server.\n", fname);
                    continue;
            }
            //If the file exists, the size in bytes is sent rather than "dne"
            int file_size = atoi(buf);
            printf("Receiving file %s from client.\nFilesize: %d\n", fname, file_size);
            FILE* f = fopen(fname, "wb");
            if(f == NULL){
                error("Error opening file for writing");
            }

            int bytes_transfered = 0;
            while(true){
                bzero(buf, sizeof(buf));
                int transfer_size = BUFSIZE;
                if(file_size - bytes_transfered < 1024){
                    //Less than 1024 bytes remaining, don't send full buffer
                    transfer_size = file_size - bytes_transfered;
                }
                n = recvfrom(sockfd, buf, sizeof(buf), 0, (struct sockaddr *)&serveraddr, &serverlen);
                if (n < 0)
                    error("ERROR in recvfrom");
                bytes_transfered += transfer_size;
                printf("Recieved %d of %d bytes\n", bytes_transfered, file_size);
                fwrite(buf, transfer_size, 1, f);
                if (bytes_transfered >= file_size){
                    printf("File transferred successfully.\n");
                    fclose(f);
                    break;
                }
            }
        }
        else if(!strncmp(cmd, "put", 3)){
            printf("filename: %s\n", fname);
            FILE* f = fopen(fname, "rb");
            if(f == NULL){
                printf("Error: file does not exist in directory\n");
                continue;
            }

            serverlen = sizeof(serveraddr);
            n = sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr *)&serveraddr, serverlen);
            if (n < 0)
                error("ERROR in sendto");
            //Calculate file size to tell server how many bytes to expect
            fseek(f, 0L, SEEK_END);
            int file_size = ftell(f);
            rewind(f);

            char file_buf[BUFSIZE];
            sprintf(file_buf, "%d", file_size);
            printf("Size of file: %s Bytes - sending size to server\n", file_buf);
            n = sendto(sockfd, file_buf, BUFSIZE, 0, (struct sockaddr *)&serveraddr, serverlen);
            if (n < 0)
                error("ERROR in sendto");
            bzero(file_buf, sizeof(file_buf));

            //Send file to server 1024 bytes at a time
            while(fread(file_buf, 1, sizeof(file_buf), f) != 0){
                n = sendto(sockfd, file_buf, sizeof(file_buf), 0, (struct sockaddr *)&serveraddr, serverlen);
                if (n < 0)
                    error("ERROR in sendto");
                bzero(file_buf, sizeof(file_buf));
            }
            fclose(f);
            printf("File %s transferred successfully\n", fname);
        }
        else if(!strncmp(cmd, "delete", 6)){
            serverlen = sizeof(serveraddr);
            n = sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr *)&serveraddr, serverlen);
            if (n < 0)
                error("ERROR in sendto");

            n = recvfrom(sockfd, buf, sizeof(buf), 0, (struct sockaddr *)&serveraddr, &serverlen);
            if (n < 0)
                error("ERROR in recvfrom");
            if(!strncmp(buf, "dne", 3)){ //File doesn't exist on server
                printf("Requested file %s doesn't exist on server.\n", fname);
            } else {
                printf("%s %s\n", buf, fname);
            }
        }
        else if(!strncmp(cmd, "ls", 2)){
            serverlen = sizeof(serveraddr);
            n = sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr *)&serveraddr, serverlen);
            if (n < 0)
                error("ERROR in sendto");

            /* print the server's reply */
            n = recvfrom(sockfd, buf, sizeof(buf), 0, (struct sockaddr *)&serveraddr, &serverlen);
            if (n < 0)
                error("ERROR in recvfrom");
            printf("%s", buf);
        }

        else if(!strncmp(cmd, "exit", 4)){
            serverlen = sizeof(serveraddr);
            n = sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr *)&serveraddr, serverlen);
            if (n < 0)
                error("ERROR in sendto");

            /* print the server's reply */
            close(sockfd);
            printf("Server terminated, exiting program.\n");
            break;
        }
        else {
            //Unknown command
            serverlen = sizeof(serveraddr);
            n = sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr *)&serveraddr, serverlen);
            if (n < 0)
                error("ERROR in sendto");

            n = recvfrom(sockfd, buf, sizeof(buf), 0, (struct sockaddr *)&serveraddr, &serverlen);
            if (n < 0)
                error("ERROR in recvfrom");
            printf("%s", buf);
        }
    }
    return 0;
}
