/* 
 * tigerS - tiger FTP server daemon
 *
 * allows for the transfer of files through tigerC 
 * in the local directory on the running host
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

#include "tigerS.h"

#define CREDFILE ("authorized.txt")

credNode head = NULL;

/*
 * read the credentials file and populate the linked list
 */
static void read_auth(void) {
    credNode localHead = NULL;
    char username[MAX_UP_STR_LEN];
    char password[MAX_UP_STR_LEN];

    FILE* filePtr = NULL;
    filePtr = fopen(CREDFILE, "r");

    if (filePtr == NULL) {
        fprintf(stdout, "\nAuthorized users file not found!\n");
        exit(-1);
    }

    // user/pass individually limited to MAX_UP_STR_LEN
    // add space seperator and eol char
    char line[MAX_UP_STR_LEN*2+2];
    credNode prevNode = NULL;
    while (fgets(line, MAX_UP_STR_LEN*2+2, filePtr) != NULL) {
        // scan the line from the file
        sscanf(line, "%s %s", username, password);

        // populate the next credential node
        credNode nextNode = (credNode) malloc(sizeof(struct credNode));
        strcpy(nextNode->username, username);
        strcpy(nextNode->password, password);
        
        // update the next node pointers
        if(localHead == NULL) {
            localHead = nextNode;
        } else {
            prevNode->next = nextNode;
            nextNode->next = NULL;
        }
        prevNode = nextNode;
    }

    // set global auth linked list
    head = localHead;
    return;
}


/*
 * check the client-provided user/pass against the linked list
 * return 1 if found, 0 if unauthorized
 */
static int auth_login(char* user, char* pass) {
    credNode node; 
    node = head;
    while(node) {
        if (strcmp(user, node->username)) {
            node = node->next;
            continue;
        }
        if (strcmp(pass, node->password) == 0) {
            return 1;
        }
        node = node->next;
    }
    return 0;
}

/*
 * handle the client connections
 */
static void* client_handler(void* socket_c) {
    int clientSock = *(int *)socket_c;

    char request[MAX_MSG_LEN];
    char reply[MAX_MSG_LEN];

    char cmd[MAX_MSG_LEN];
    char delims[] = " \t\r\n";
    char *str;
    char *strPtr;

    char user[MAX_UP_STR_LEN];
    char pass[MAX_UP_STR_LEN];

    int authorized = 0;

    char filename[MAX_MSG_LEN];
    int fd, fileSize;
    struct stat fileStat;
    char *fileptr;

    while(1) {
        recv(clientSock, request, MAX_MSG_LEN, 0);

        str = strtok_r(request, delims, &strPtr);
        strncpy(cmd, str, MAX_MSG_LEN);

        // tconnect <ip> <username> <password>
        if (strcmp(cmd, "tconnect") == 0) {

            // ip used by client
            str = strtok_r(NULL, delims, &strPtr);

            // get username
            str = strtok_r(NULL, delims, &strPtr);
            strncpy(user, str, MAX_UP_STR_LEN);

            // get password 
            str = strtok_r(NULL, delims, &strPtr);
            if (str) {
                strncpy(pass, str, MAX_UP_STR_LEN);
            }
            else {
                strncpy(pass, "", MAX_MSG_LEN);;
            }

            authorized = auth_login(user, pass);

            if (authorized) {
                fprintf(stdout, "User %s now connected\n", user);
                strcpy(reply, PASS);
                send(clientSock, reply, MAX_MSG_LEN, 0);
            }
            else {
                fprintf(stdout, "User %s failed authentication\n", user);
                strcpy(reply, UNAUTH);
                send(clientSock, reply, MAX_MSG_LEN, 0);
                //close(clientSock);
            }
        }

        // tget <file>
        else if (strcmp(cmd, "tget") == 0) {
            
            // get filename
            str = strtok_r(NULL, delims, &strPtr);
            strncpy(filename, str, MAX_MSG_LEN);

            if (!authorized) {
                strcpy(reply, UNAUTH);
                send(clientSock, reply, MAX_MSG_LEN, 0);
            }
            else {
                fprintf(stdout, "%s is attempting to retrieve file: %s\n", user, filename);

                fd = open(filename, O_RDONLY);
                // file exists
                if (fd >= 0) {
                    // notify the socket
                    strcpy(reply, PASS);
                    send(clientSock, reply, MAX_MSG_LEN, 0);

                    // send the pending file's size
                    stat(filename, &fileStat);
                    fileSize = fileStat.st_size;

                    // send file size
                    send(clientSock, &fileSize, sizeof(int), 0);

                    // send the file
                    sendfile(clientSock, fd, NULL, fileSize);

                    // wait to confirm from client
                    recv(clientSock, reply, MAX_MSG_LEN, 0);
                    if (strcmp(reply, PASS) == 0) {
                        fprintf(stdout, "%s sent succesfully to %s\n", filename, user);
                    }
                    else {
                        fprintf(stdout, "Sending %s to %s failed\n", filename, user);
                    }

                }
                else {
                    fprintf(stdout, "%s does not exist\n", filename);
                    strcpy(reply, FAIL);
                    send(clientSock, reply, MAX_MSG_LEN, 0);
                }
            }
        }

        // tput <file>
        else if (strcmp(cmd, "tput") == 0) {

            if (authorized) {
                // notify client it can do this
                strcpy(reply, PASS);
                send(clientSock, reply, MAX_MSG_LEN, 0);

                // get filename for log
                str = strtok_r(NULL, delims, &strPtr);
                strncpy(filename, str, MAX_MSG_LEN);

                // get the incoming file's size
                recv(clientSock, &fileSize, sizeof(int), 0);

                // allocate space for the file
                fileptr = malloc(fileSize);

                // create the file descriptor on the fs
                fd = open(str, O_CREAT | O_WRONLY, 0666);

                // get the file from the client
                ssize_t bytesRecv;
                bytesRecv = recv(clientSock, fileptr, fileSize, MSG_WAITALL);

                if (bytesRecv != fileSize) {
                    send(clientSock, FAIL, MAX_MSG_LEN, 0);
                    fprintf(stdout, "Failed to retrieve %s", filename);
                }
                else {
                    // write the file to the fs
                    write(fd, fileptr, fileSize);

                    // notify client file has been received 
                    send(clientSock, PASS, MAX_MSG_LEN, 0);

                    fprintf(stdout, "Received %s from %s\n", filename, user);
                }

                // free it from memory and close it
                free(fileptr);
                close(fd);
            }
            else {
                strcpy(reply, UNAUTH);
                send(clientSock, reply, MAX_MSG_LEN, 0);
            }
        }

        // tclose
        else if (strcmp(cmd, "tclose") == 0) {
            if (authorized) {
               fprintf(stdout, "%s quit\n", user);
            }
            close(clientSock);
            return 0;
        }

        // command not found
        else {
            strcpy(reply, FAIL);
            send(clientSock, reply, MAX_MSG_LEN, 0);
        }
    }

}

/*
 * start the server socket and begin listening for clients
 */
int main() {
    fprintf(stdout, "Initializing TigerFTP\n\n");
    fprintf(stdout, "Reading authorized users list... ");
    read_auth();
    fprintf(stdout, "OK\n");

    // set up server sockets address
    struct sockaddr_in serverAddr;

    serverAddr.sin_family = AF_INET;
    inet_pton(AF_INET, SERVER_IP, &serverAddr.sin_addr);
    serverAddr.sin_port = (unsigned short)PORT;

    fprintf(stdout, "Initializing server socket... ");
    int serverSock = socket(AF_INET, SOCK_STREAM, 0);

    // catch error
    if (serverSock == -1) {
        fprintf(stdout, "\nFailed to initalize server socket");
        fprintf(stdout, "\n%d: %s\n", errno, strerror(errno));
        exit(1);
    }
    fprintf(stdout, "OK\n");

    fprintf(stdout, "Binding server socket to address... ");
    int sockReturn = bind(serverSock, (struct sockaddr*)&serverAddr, sizeof(serverAddr));

    // catch error
    if (sockReturn == -1) {
        fprintf(stdout, "\nFailed to bind server socket");
        fprintf(stdout, "\n%d: %s\n", errno, strerror(errno));
        exit(1);
    }
    fprintf(stdout, "OK\n");
    
    // set server_sock passive with a queue limit of 5
    fprintf(stdout, "Begin listening... ");
    sockReturn = listen(serverSock, 5);

    if (sockReturn == -1) {
        fprintf(stdout, "\nFailed to begin listening");
        fprintf(stdout, "\n%d: %s\n", errno, strerror(errno));
        exit(1);
    }
    fprintf(stdout, "OK\n");

    fprintf(stdout, "\nTigerFTP server successfully initialized\n\n");

    // client socket stuff
    int clientSock;
    struct sockaddr_in clientAddr;
    socklen_t clientAddrSize = sizeof(clientAddr);
    int *newSock;

    // start accepting clients ad infinitum 
    while( (clientSock = accept(serverSock, (struct sockaddr*)&clientAddr, &clientAddrSize)) ) {
        // spawn a new thread for each one
        pthread_t th;
        newSock = malloc(sizeof(int *));
        *newSock = clientSock;
        pthread_create(&th, NULL, client_handler, (void *) newSock);

        if (clientSock < 0) {
            fprintf(stdout, "Failed to accept client\n");
        }
    }

    return 0;
}
