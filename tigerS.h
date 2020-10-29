#ifndef SERVER_H
#define SERVER H

// cap usernames and passwords 
#define MAX_UP_STR_LEN 25

// cap interserver message length
#define MAX_MSG_LEN 255

#define PORT (11700)
#define SERVER_IP ("127.0.0.1")

// reply primitives
#define PASS ("1")
#define UNAUTH ("2")
#define FAIL ("3")

struct credNode {
    char username[MAX_MSG_LEN];
    char password[MAX_MSG_LEN];
    struct credNode* next;
};
typedef struct credNode *credNode;

#endif
