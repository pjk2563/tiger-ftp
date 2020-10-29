/* 
 * tigerc - tiger FTP server client
 *
 * allows for the transfer of files with tigerS
 * in the local directory on the remote host
 * 
 * Paul Kelly
 * October 26th, 2020
 */
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <pthread.h>

#include <netinet/in.h>
#include <netinet/ip.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/sendfile.h>
#include <dirent.h>

#include "tigerC.h"

int CONNECTED = 0;
char USERNAME[MAX_UP_STR_LEN];

/*
 * opens a basic connection to the server socket
 * connect <server ip>
 * sets CONNECTED on success
 */
static int rconnect(char* ip, int *clientSock) {

    fprintf(stdout, "Initializing client socket... ");
    *clientSock = socket(AF_INET, SOCK_STREAM, 0);

    // catch error
    if (*clientSock == -1) {
        fprintf(stdout, "\nFailed to initalize client socket");
        fprintf(stdout, "\n%d: %s\n", errno, strerror(errno));
        return -1;
    }
    fprintf(stdout, "OK\n");

    // set up server socket address struct
    struct sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &(serverAddr.sin_addr));
    serverAddr.sin_port = (unsigned short)PORT;

    fprintf(stdout, "Connecting to server at %s... ", ip);
    int sockReturn = connect(*clientSock, (struct sockaddr*)&serverAddr, sizeof(serverAddr));

    // catch error
    if (sockReturn == -1) {
        fprintf(stdout, "\nFailed to connect to server");
        fprintf(stdout, "\n%d: %s\n", errno, strerror(errno));
        return -1;
    }
    fprintf(stdout, "OK\n");

    fprintf(stdout, "TigerFTP client connected successfully\n\n");
    CONNECTED = 1;

    return 0;
}

/* 
 * opens socket connection and attempts to log in
 * tconnect <server ip> <username> <password>
 *
 * returns pointer to username on success
 */
static void tconnect(char *cmd, int *clientSock) {
   // back up the original command
   char ogCmd[MAX_MSG_LEN];
   strncpy(ogCmd, cmd, MAX_MSG_LEN);

   char delims[] = " \t\r\n";
   char *str;

   // ignore tconnect 
   str = strtok(cmd, delims);

   // get the ip from the command
   char ip[MAX_MSG_LEN];
   str = strtok(NULL, delims);
   strncpy(ip, str, MAX_MSG_LEN);

   // connect to the socket, return on failure
   if (!CONNECTED) {
       if (rconnect(ip, clientSock) < 0) {
           return;
       }
   }

   // get the username for returning
   char user[MAX_UP_STR_LEN];
   str = strtok(NULL, delims);
   if (str == NULL) {
       fprintf(stdout, "Incorrect parameters\n");
       return;
   }
   strncpy(user, str, MAX_UP_STR_LEN);

   // send unadulterated command to server to login 
   send(*clientSock, ogCmd, MAX_MSG_LEN, 0);

   char reply[MAX_MSG_LEN];
   recv(*clientSock, reply, MAX_MSG_LEN, 0);
   if (strcmp(reply, PASS) == 0) {
       fprintf(stdout, "Now logged in as: %s\n", user);
       strncpy(USERNAME, user, MAX_MSG_LEN);
   }
   else {
       fprintf(stdout, "Login failed\n");
       CONNECTED = 0;
   }

   return;
}

/*
 * tells server to close the socket connection
 * tclose
 */
static void tclose(int *clientSock){
    send(*clientSock, "tclose", MAX_MSG_LEN, 0);
    fprintf(stdout, "Disconnected\n");
    CONNECTED = 0;
    return;
}

/*
 * gets file from server
 * tget <file>
 */
static void tget(char *cmd, int *clientSock) {
   // back up the original command
   char ogCmd[MAX_MSG_LEN];
   strncpy(ogCmd, cmd, MAX_MSG_LEN);

   char *str;
   char delims[] = " \t\r\n";

   // throw out tget
   str = strtok(cmd, delims);

   char filename[MAX_MSG_LEN];

   // get filename
   str = strtok(NULL, delims);
   strncpy(filename, str, MAX_MSG_LEN);

   // talk to the server
   send(*clientSock, ogCmd, MAX_MSG_LEN, 0);
   char reply[MAX_MSG_LEN];
   recv(*clientSock, reply, MAX_MSG_LEN, 0);

   if (strcmp(reply, PASS) == 0) {
       // auth is good, file is available
       int fd, fileSize;
       char *fileptr;

       // get the incoming file's size
       recv(*clientSock, &fileSize, sizeof(int), 0);

       // allocate space for the file
       fileptr = malloc(fileSize);

       // create the file descriptor on the fs
       fd = open(filename, O_CREAT | O_WRONLY, 0666);

       // get the file from the server
       ssize_t bytesRecv;
       bytesRecv = recv(*clientSock, fileptr, fileSize, MSG_WAITALL);

       // tell the server the file was received or failed
       if (bytesRecv != fileSize) {
           send(*clientSock, FAIL, MAX_MSG_LEN, 0);
           fprintf(stdout, "Failed to download %s\n", filename);
       }
       else {
           send(*clientSock, PASS, MAX_MSG_LEN, 0);

           // write the file to the fs
           write(fd, fileptr, fileSize);
           fprintf(stdout, "Downloaded %s\n", filename);
       }

       // free it from memory and close it
       free(fileptr);
       close(fd);
   }
   else if (strcmp(reply, UNAUTH) == 0) {
       fprintf(stdout, "Unauthorized\n");
   }
   else {
       fprintf(stdout, "File does not exist on the server\n");
   }

   return;
}

/*
 * puts local file on server
 * tput <file>
 */
static void tput(char *cmd, int *clientSock) {
   // back up the original command
   char ogCmd[MAX_MSG_LEN];
   strncpy(ogCmd, cmd, MAX_MSG_LEN);

   char *str;
   char delims[] = " \t\r\n";

   // throw out tput
   str = strtok(cmd, delims);

   char filename[MAX_MSG_LEN];

   // get filename
   str = strtok(NULL, delims);
   strncpy(filename, str, MAX_MSG_LEN);

   fprintf(stdout, "Sending %s...", filename);

   int fd = open(filename, O_RDONLY);
   // if the file exists
   if (fd >= 0) {
       // notify the server
       send(*clientSock, ogCmd, MAX_MSG_LEN, 0);
       char reply[MAX_MSG_LEN];
       recv(*clientSock, reply, MAX_MSG_LEN, 0);

       if (strcmp(reply, UNAUTH) == 0) {
           fprintf(stdout, "\nUnauthorized\n");
           return;
       }

       // send relevant data
       int fileSize;
       struct stat fileStat;

       stat(filename, &fileStat);
       fileSize = fileStat.st_size;

       // send file size
       send(*clientSock, &fileSize, sizeof(int), 0);

       // send the file to the server
       sendfile(*clientSock, fd, NULL, fileSize);
       
       // wait for server to acknowledge receipt
       recv(*clientSock, reply, MAX_MSG_LEN, 0);

       if (strcmp(reply, PASS) == 0) {
           fprintf(stdout, "OK\n%s sent successfully\n", filename);
       }
       else {
           fprintf(stdout, "FAIL\n%s failed to send\n", filename);
       }
   }
   else {
       fprintf(stdout, "\n%s does not exist\n", filename);
   }
   return;
}

/*
 * run the tigerC client!
 */
int main() {
    CONNECTED = 0;
    strncpy(USERNAME, "Not logged in", MAX_MSG_LEN);
    
    int clientSock;

    char cmd[MAX_MSG_LEN];
    char ogCmd[MAX_MSG_LEN];
    char *str;
    char delims[] = " \t\r\n";

    fprintf(stdout, "Welcome to tigerFTP!\n");

    fprintf(stdout, "tigerFTP> ");

    while( fgets(cmd, MAX_MSG_LEN, stdin) ) {
        // backup the command 
        strncpy(ogCmd, cmd, MAX_MSG_LEN);
        
        // find out which function to run
        str = strtok(cmd, delims);

        // tconnect
        if (strcmp(str, "tconnect") == 0) {
            str = strtok(NULL, delims);

            if (str) {
                if(strcmp(str, "help") == 0) {
                    fprintf(stdout, "\nUsage: tconnect <server ip> <username> <password>\n");
                    fprintf(stdout, " Connect and authenticate to server\n");
                }
                else {
                    tconnect(ogCmd, &clientSock);
                }
            }
            else {
                fprintf(stdout, "Incorrect parameters\n");
            }
        }

        // rconnect (for debugging)
        else if (strcmp(str, "rconnect") == 0) {
            str = strtok(NULL, delims);

            if (str) {
                if(strcmp(str, "help") == 0) {
                    fprintf(stdout, "\nUsage: rconnect <server ip>\n");
                    fprintf(stdout, " Connect to server without authenticating\n");
                    fprintf(stdout, " (raw connect)\n");
                }
                else {
                    if (CONNECTED) {
                        fprintf(stdout, "Already connected");
                    }
                    else {
                        // get ip from command
                        char ip[MAX_MSG_LEN];
                        strncpy(ip, str, MAX_MSG_LEN);

                        rconnect(ip, &clientSock);
                    }
                }
            }
            else {
                fprintf(stdout, "Incorrect parameters\n");
            }
        }

        // tget 
        else if (strcmp(str, "tget") == 0) {
            str = strtok(NULL, delims);

            if (str) {
                if(strcmp(str, "help") == 0) {
                    fprintf(stdout, "\nUsage: tget <file name>\n");
                    fprintf(stdout, " Download file from server\n");
                }
                else {
                    if(!CONNECTED) {
                        fprintf(stdout, "Not connected\n");
                    }
                    else {
                        tget(ogCmd, &clientSock);
                    }
                }
            }
            else {
                fprintf(stdout, "Incorrect parameters\n");
            }
        }

        // tput
        else if (strcmp(str, "tput") == 0) {
            str = strtok(NULL, delims);

            if (str) {
                if(strcmp(str, "help") == 0) {
                    fprintf(stdout, "\nUsage: tput <file name>\n");
                    fprintf(stdout, " Upload file to server\n");
                }
                else {
                    if (!CONNECTED) {
                        fprintf(stdout, "Not connected\n");
                    }
                    else {
                       tput(ogCmd, &clientSock);
                    }
                }
            }
            else {
                fprintf(stdout, "Incorrect parameters\n");
            }
        }

        // tclose 
        else if (strcmp(str, "tclose") == 0) {
            str = strtok(NULL, delims);

            if (str) {
                fprintf(stdout, "\nUsage: tclose\n");
                fprintf(stdout, " Disconnect from server\n");
            }
            else if (CONNECTED) {
                tclose(&clientSock);
                strncpy(USERNAME, "Not logged in", MAX_MSG_LEN);
            }
            else {
                fprintf(stdout, "Not connected\n");
            }
        }

        // help
        else if (strcmp(str, "help") == 0) {
            // general help message
            fprintf(stdout, "\ntigerC - a basic ftp client\n");
            fprintf(stdout, "Run \'<cmd> help\' for more information. Commands are:\n\n");
            fprintf(stdout, " rconnect \n");
            fprintf(stdout, " tconnect \n");
            fprintf(stdout, " tget \n");
            fprintf(stdout, " tput \n");
            fprintf(stdout, " tclose \n");
            fprintf(stdout, " whoami \n");
            fprintf(stdout, " quit\n\n");
        }

        else if (strcmp(str, "whoami") == 0) {
            str = strtok(NULL, delims);

            if (str) {
                fprintf(stdout, "\nUsage: whoami\n");
                fprintf(stdout, " Show currently-authenticated login\n");
            }
            else {
                fprintf(stdout, "%s\n", USERNAME);
            }
        }

        else if (strcmp(str, "quit") == 0) {
            str = strtok(NULL, delims);

            if (str) {
                fprintf(stdout, "\nUsage: quit\n");
                fprintf(stdout, " Quit  TigerFTP client\n");
            }
            else {
                if (CONNECTED) {
                  tclose(&clientSock);
                }
                fprintf(stdout, "Goodbye\n");
                exit(0);
            }
        }

        else {
            fprintf(stdout, "Command not recognized\n");
        }

        fprintf(stdout, "tigerFTP> ");

    }
}


