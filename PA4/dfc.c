#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h>
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
#define MAXFILES 128
#define MAXFILENAME 64

#define DFS1 0
#define DFS2 1
#define DFS3 2
#define DFS4 3

char user[BUF];
char pass[BUF];
int sockfd[4];
struct sockaddr_in serveraddr[4];
bool serverUp = {true, true, true, true};

void initServerDirs();
void createSocket(int dfsIdx, int port);
int sendtoDFS(char* msg, int dfsIdx, size_t len);
int hashBucket(char* fname);
void sigpipe_handler(int signum);
void sendChunk(char* chunk, char* fname, char chunkChar, int dfsIdx, size_t len);
int auth(char* user, char* pass, int dfsIdx);
void printFiles(char* f1, char* f2, char* f3, char*f4);
int hasChunk(char* fname, int chunk, int dfsIdx);
void getChunk(int dfsIdx, int chunkSize, FILE* f);

int main(int argc, char** argv){
  int port1, port2, port3, port4, n;
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

  signal(SIGPIPE, sigpipe_handler); //! Writes to a terminated server would cause a sigpipe signal, ignoring with custom handler

  // createSocket(DFS1, port1);
  // createSocket(DFS2, port2);
  // createSocket(DFS3, port3);
  // createSocket(DFS4, port4);

  while(1){ //Program loop

    bzero(userInput, BUF);
    printf("\nEnter one of the following commands:\n * get [filename]\n * put [filename]\n * ls\n> ");
    fgets(userInput, BUF, stdin);
    sscanf(userInput, "%s %s", cmd, fname);

    createSocket(DFS1, port1);
    createSocket(DFS2, port2);
    createSocket(DFS3, port3);
    createSocket(DFS4, port4);

    if(!strcmp(cmd, "put")){
      struct stat s;
      ssize_t fsize;
      if(stat(fname, &s) != 0){
        printf("File %s does not exist.\n", fname);
        continue;
      }
      int bucket = hashBucket(fname);
      printf("Hash mod 4 is: %d\n", bucket);

      //Calculate file size
      FILE* f = fopen(fname, "rb");
      fseek(f, 0, SEEK_END);
      fsize = ftell(f);
      rewind(f);

      char p1[fsize/4];
      char p2[fsize/4];
      char p3[fsize/4];
      char p4[fsize - 3*(fsize/4)];

      fread(p1, sizeof(p1), 1, f);
      fread(p2, sizeof(p2), 1, f);
      fread(p3, sizeof(p3), 1, f);
      fread(p4, sizeof(p4), 1, f);
      //printf("Piece 1: %s\nPiece 2: %s\nPiece 3: %s\nPiece 4: %s\n", p1, p2, p3, p4);

      fclose(f);
      if(bucket == 0){
        sendChunk(p1, fname, '1', DFS1, sizeof(p1));
        sendChunk(p2, fname, '2', DFS1, sizeof(p2));
        sendChunk(p2, fname, '2', DFS2, sizeof(p2));
        sendChunk(p3, fname, '3', DFS2, sizeof(p3));
        sendChunk(p3, fname, '3', DFS3, sizeof(p3));
        sendChunk(p4, fname, '4', DFS3, sizeof(p4));
        sendChunk(p4, fname, '4', DFS4, sizeof(p4));
        sendChunk(p1, fname, '1', DFS4, sizeof(p1));
      } else if(bucket == 1){
        sendChunk(p4, fname, '4', DFS1, sizeof(p4));
        sendChunk(p1, fname, '1', DFS1, sizeof(p1));
        sendChunk(p1, fname, '1', DFS2, sizeof(p1));
        sendChunk(p2, fname, '2', DFS2, sizeof(p2));
        sendChunk(p2, fname, '2', DFS3, sizeof(p2));
        sendChunk(p3, fname, '3', DFS3, sizeof(p3));
        sendChunk(p3, fname, '3', DFS4, sizeof(p3));
        sendChunk(p4, fname, '4', DFS4, sizeof(p4));
      } else if(bucket == 2){
        sendChunk(p3, fname, '3', DFS1, sizeof(p3));
        sendChunk(p4, fname, '4', DFS1, sizeof(p4));
        sendChunk(p4, fname, '4', DFS2, sizeof(p4));
        sendChunk(p1, fname, '1', DFS2, sizeof(p1));
        sendChunk(p1, fname, '1', DFS3, sizeof(p1));
        sendChunk(p2, fname, '2', DFS3, sizeof(p2));
        sendChunk(p2, fname, '2', DFS4, sizeof(p2));
        sendChunk(p3, fname, '3', DFS4, sizeof(p3));
      } else {
        sendChunk(p2, fname, '2', DFS1, sizeof(p2));
        sendChunk(p3, fname, '3', DFS1, sizeof(p3));
        sendChunk(p3, fname, '3', DFS2, sizeof(p3));
        sendChunk(p4, fname, '4', DFS2, sizeof(p4));
        sendChunk(p4, fname, '4', DFS3, sizeof(p4));
        sendChunk(p1, fname, '1', DFS3, sizeof(p1));
        sendChunk(p1, fname, '1', DFS4, sizeof(p1));
        sendChunk(p2, fname, '2', DFS4, sizeof(p2));
      }
    }

    else if (!strcmp(cmd, "ls")){
      char files1[MAXLINE];
      char files2[MAXLINE];
      char files3[MAXLINE];
      char files4[MAXLINE];
      memset(files1, 0, MAXLINE);
      memset(files2, 0, MAXLINE);
      memset(files3, 0, MAXLINE);
      memset(files4, 0, MAXLINE);

      int authval = auth(user, pass, DFS1);
      if(authval == 1){
        sendtoDFS("ls", DFS1, 2);
        n = read(sockfd[DFS1], files1, MAXLINE);
        if(n <= 0){
          printf("DFS1 is down.\n");
        }

        if(!strcmp(files1, "nf")){
          printf("No files on DFS1.\n");
          memset(files1, 0, MAXLINE);
        }
      } else if(authval == 0){
        printf("Authentication failed.\n");
        continue;
      }

      authval = auth(user, pass, DFS2);
      if(authval == 1){
        sendtoDFS("ls", DFS2, 2);
        n = read(sockfd[DFS2], files2, MAXLINE);
        if(n <= 0){
          printf("DFS2 is down.\n");
        }

        if(!strcmp(files2, "nf")){
          printf("No files on DFS2.\n");
          memset(files2, 0, MAXLINE);
        }
      } else if(authval == 0){
        printf("Authentication failed.\n");
        continue;
      }

      authval = auth(user, pass, DFS3);
      if(authval == 1){
        sendtoDFS("ls", DFS3, 2);
        n = read(sockfd[DFS3], files3, MAXLINE);
        if(n <= 0){
          printf("DFS3 is down.\n");
        }

        if(!strcmp(files3, "nf")){
          printf("No files on DFS3.\n");
          memset(files3, 0, MAXLINE);
        }
      } else if(authval == 0){
        printf("Authentication failed.\n");
        continue;
      }

      authval = auth(user, pass, DFS4);
      if(authval == 1){
        sendtoDFS("ls", DFS4, 2);
        n = read(sockfd[DFS4], files4, MAXLINE);
        if(n <= 0){
          printf("DFS4 is down.\n");
        }

        if(!strcmp(files4, "nf")){
          printf("No files on DFS4.\n");
          memset(files4, 0, MAXLINE);
        }
      } else if(authval == 0){
        printf("Authentication failed.\n");
        continue;
      }

    printFiles(files1, files2, files3, files4);
    }

    else if (!strcmp(cmd, "get")){
      if(auth(user, pass, DFS1) == 0){
        printf("Authentication failed.\n");
        continue;
      }
      if(auth(user, pass, DFS2) == 0){
        printf("Authentication failed.\n");
        continue;
      }
      if(auth(user, pass, DFS3) == 0){
        printf("Authentication failed.\n");
        continue;
      }
      if(auth(user, pass, DFS4) == 0){
        printf("Authentication failed.\n");
        continue;
      }

      FILE* f;
      f = fopen(fname, "wb");
      if(!f){
        printf("Error opening file %s\n", fname);
        continue;
      }
      for(int chonk = 1; chonk < 5; chonk++){ //For each chunk
        bool chunkExists = false;
        for(int dfs = 0; dfs < 4; dfs++){ //For each server
          int chunkSize = hasChunk(fname, chonk, dfs);
          if(chunkSize > 0){
            chunkExists = true;
            getChunk(dfs, chunkSize, f);
            break;
          }
          if(chunkSize == 0){
            //printf("The requested file does not exist on DFS%d", dfs+1);
          }
        }
        if(!chunkExists){
          printf("File is incomplete.\n");
          remove(fname);
          break;
        }
      }
      fclose(f);
    }

    else {
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
  struct timeval timeout;
  timeout.tv_sec = 1;
  timeout.tv_usec = 0;

  sockfd[dfsIdx] = socket(AF_INET, SOCK_STREAM, 0);
  if(sockfd[dfsIdx] < 0){
    printf("Error creating socket\n");
    exit(1);
  }

  if(setsockopt(sockfd[dfsIdx], SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout)) < 0){
    printf("Error setting timeout\n");
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
    //printf("Error in connect: errno %d\n", errno);
    //exit(1);
  }
}

//Returns 0 if the file does not exist on the dfs, else returns file size
int hasChunk(char* fname, int chunk, int dfsIdx){
  int n;
  char cmdbuf[strlen(fname) + strlen("get ") + strlen(".1 ")];
  memset(cmdbuf, 0, sizeof(cmdbuf));
  char buf[BUF];
  char chunkChar = chunk + '0';
  strcpy(cmdbuf, "get ");
  strcat(cmdbuf, fname);
  strcat(cmdbuf, ".");
  strncat(cmdbuf, &chunkChar, 1);

  printf("Sending command: %s\n", cmdbuf);

  sendtoDFS(cmdbuf, dfsIdx, strlen(cmdbuf));

  memset(buf, 0, sizeof(buf));
  n = read(sockfd[dfsIdx], buf, BUF);
  if(n <= 0){
    printf("DFS%d is down.\n", dfsIdx+1);
    return -1;
  }
  if(!strcmp(buf, "dne")){
    printf("File %s is not present on DFS%d\n", fname, dfsIdx+1);
    return 0;
  }
  printf("FILE EXISTS, SIZE %d BYTES\n", atoi(buf));
  return atoi(buf);
}

void getChunk(int dfsIdx, int chunkSize, FILE* f){
  char fileChunk[chunkSize];
  memset(fileChunk, 0, chunkSize);
  write(sockfd[dfsIdx], "ready", 5);
  read(sockfd[dfsIdx], fileChunk, chunkSize);
  printf("Received from server####:\n%s\n", fileChunk);
  fwrite(fileChunk, 1, chunkSize, f);
}

void sendChunk(char* chunk, char* fname, char chunkChar, int dfsIdx, size_t len){
  char fsize[BUF];
  sprintf(fsize, "%d", (int)len);
  char cmd[strlen(fname) + strlen("put ") + strlen(fsize) + 5]; //5 additional bytes for . prefix, .{1-4} suffix, and null terminator
  int authRetval = auth(user, pass, dfsIdx);
  if(authRetval != 1){
    printf("Authentication failed.\n");
    return;
  }

  strcpy(cmd, "put:");
  strcat(cmd, fsize);
  strcat(cmd, " ");
  strcat(cmd, fname);
  strcat(cmd, ".");
  strncat(cmd, &chunkChar, 1);
  printf("Sending command to DFS%d: %s\n", dfsIdx+1, cmd);

  sendtoDFS(cmd, dfsIdx, strlen(cmd) + 1);

  printf("Sending chunk, size: %d\n", (int)len);
  sendtoDFS(chunk, dfsIdx, len);
  printf("Sent chunk\n");
}

void printFiles(char* f1, char* f2, char* f3, char* f4){
  char* tok;
  int numFiles = 0;
  char files[MAXFILES][MAXFILENAME]; //Stores a list of files
  memset(files, 0, sizeof(files));
  int fileParts[MAXFILES]; //Stores parts of file based on index in files table
  memset(fileParts, 0, sizeof(fileParts));


  if(strlen(f1) != 0){
    tok = strtok(f1, "\n");
    while(tok != NULL){
      bool newFile = true;
      //printf("tok: %s\n", tok);
      char* chunk = strrchr(tok, '.')+1;
      //printf("File chunk %s\n", chunk);
      *(chunk-1) = '\0';

      for(int i = 0; i < numFiles; i++){
        if(!strcmp(tok, files[i])){
          //printf("Found match for %s\n", tok);
          newFile = false;
          fileParts[i] |= 1<<(atoi(chunk)-1);
          break;
        }
      }

      if(newFile){
        //printf("Adding file %s\n", tok);
        strcpy(files[numFiles], tok);
        fileParts[numFiles] |= 1<<(atoi(chunk)-1);
        numFiles++;
      }

      tok = strtok(NULL, "\n");
    }
  }

  if(strlen(f2) != 0){
    tok = strtok(f2, "\n");
    while(tok != NULL){
      bool newFile = true;
      //printf("tok: %s\n", tok);
      char* chunk = strrchr(tok, '.')+1;
      //printf("File chunk %s\n", chunk);
      *(chunk-1) = '\0';

      for(int i = 0; i < numFiles; i++){
        if(!strcmp(tok, files[i])){
          //printf("Found match for %s\n", tok);
          newFile = false;
          fileParts[i] |= 1<<(atoi(chunk)-1);
          break;
        }
      }

      if(newFile){
        //printf("Adding file %s\n", tok);
        strcpy(files[numFiles], tok);
        fileParts[numFiles] |= 1<<(atoi(chunk)-1);
        numFiles++;
      }

      tok = strtok(NULL, "\n");
    }
  }

  if(strlen(f3) != 0){
    tok = strtok(f3, "\n");
    while(tok != NULL){
      bool newFile = true;
      //printf("tok: %s\n", tok);
      char* chunk = strrchr(tok, '.')+1;
      //printf("File chunk %s\n", chunk);
      *(chunk-1) = '\0';

      for(int i = 0; i < numFiles; i++){
        if(!strcmp(tok, files[i])){
          //printf("Found match for %s\n", tok);
          newFile = false;
          fileParts[i] |= 1<<(atoi(chunk)-1);
          break;
        }
      }

      if(newFile){
        //printf("Adding file %s\n", tok);
        strcpy(files[numFiles], tok);
        fileParts[numFiles] |= 1<<(atoi(chunk)-1);
        numFiles++;
      }

      tok = strtok(NULL, "\n");
    }
  }

  if(strlen(f4) != 0){
    tok = strtok(f4, "\n");
    while(tok != NULL){
      bool newFile = true;
      //printf("tok: %s\n", tok);
      char* chunk = strrchr(tok, '.')+1;
      //printf("File chunk %s\n", chunk);
      *(chunk-1) = '\0';

      for(int i = 0; i < numFiles; i++){
        if(!strcmp(tok, files[i])){
          //printf("Found match for %s\n", tok);
          newFile = false;
          fileParts[i] |= 1<<(atoi(chunk)-1);
          break;
        }
      }

      if(newFile){
        //printf("Adding file %s\n", tok);
        strcpy(files[numFiles], tok);
        fileParts[numFiles] |= 1<<(atoi(chunk)-1);
        numFiles++;
      }

      tok = strtok(NULL, "\n");
    }
  }

  for(int i = 0; i < numFiles; i++){
    printf("%s", files[i]);
    if(fileParts[i] != 15){ //Binary 1111, indicating all 4 parts are present
      printf(" [incomplete]");
    }
    printf("\n");
  }
}

int auth(char* user, char* pass, int dfsIdx){
  char authmsg[strlen("auth ") + strlen(user) + strlen(pass) + 2]; //2 additional bytes for ':' and null terminator
  char response[BUF];
  bzero(response, BUF);
  ssize_t n;

  //connect(sockfd[dfsIdx], (struct sockaddr*)&serveraddr[dfsIdx], sizeof(struct sockaddr_in));

  strcpy(authmsg, "auth ");
  strcat(authmsg, user);
  strcat(authmsg, ":");
  strcat(authmsg, pass);
  //printf("Sending auth message to DFS%d: %s\n", dfsIdx+1, authmsg);
  sendtoDFS(authmsg, dfsIdx, sizeof(authmsg));
  n = read(sockfd[dfsIdx], response, BUF);
  if(n <= 0){
    printf("Auth error: DFS%d is down.\n", dfsIdx+1);
    return -1;
  }
  if(!strcmp(response, "auth")){
    //printf("User %s successfully authenticated\n", user);
    return 1;
  } else {
    printf("Authentication failed for user %s\n", user);
    return 0;
  }
}

//! Sends msg to specified DFS
int sendtoDFS(char* msg, int dfsIdx, size_t len){
  int n;

  n = write(sockfd[dfsIdx], msg, len);
  if(n < 0){
    //printf("Error in sendto: errno %d\n", errno);
  }
  return n;
}

void sigpipe_handler(int signum){

}

//! Returns md5sum % 4 of file fname, implementation from https://stackoverflow.com/questions/10324611/how-to-calculate-the-md5-hash-of-a-large-file-in-c
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

  fclose(f);
  return modResult;
}
