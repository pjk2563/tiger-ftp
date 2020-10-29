#ifndef CLIENT_H 
#define CLIENT H

/*
 *  Note: This file can be replaced by TigerS.h, but in the spirit
 *        of allowing a functional client and server to be compiled
 *        seperately, these defines have been copied to avoid bugs.
 */

// cap usernames and passwords 
#define MAX_UP_STR_LEN 25

// cap interserver message length
#define MAX_MSG_LEN 255

#define PORT (11700)

// reply primitives
#define PASS ("1")
#define UNAUTH ("2")
#define FAIL ("3")

#endif
