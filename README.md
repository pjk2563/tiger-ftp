# TigerFTP 
A basic ftp client

## Installation

Use make to build both the tigerS server and tigerC client programs.

```bash
make all
```

To just make the client, use tigerC.

```bash
make tigerC
```

Alternatively,

```bash
make client
```

To just make the server, 

```bash
make tigerS
````

Alternatively,

```bash
make server
````

By default, the server and client binaries are placed in their
respective ./client/ and ./server/ subdirectories. If these
directories do not exist, they will be made. This includes the
testing binaries created by dupe.sh.

To uninstall the program,

```bash
make clean
```

## Server
Once the tigerS binary has been built, the server is run by simply
executing it: 

```bash
./tigerS
```

tigerS serves the current running directory of files. When run, it 
prints a log of connected users and transferred files to stdout.

To kill tigerS, type ctrl-C.

To configure the listen port and server IP address, see tigerS.h. 
-0.0.0.0    listens to any incoming IP address
-127.0.0.1  only listens on the current host 

The default port chosen for this project is 11700, but can be
changed in the aforementioned header file. Note, this must also be 
changed in the client header file. 

The authorized.txt file contains all of the allowed users to connect
to the tigerS FTP server. The format for the file is:

```
username password
```

Two full words, single space between, with every new line being new 
credentials. tigerS does not support comments in the authorized text 
file. Usernames and passwords are capped at 25 characters each, but
this limit can be changed by modifying the MAX_UP_STR_LEN define in
tigerS.h

The authorized.txt file should be kept in the same directory as
tigerS. If this file is missing, tigerS will not run. 

## Client
Once the tigerC binary has been built, the client is likewise run
by simply executing it:

```bash
./tigerC
```

tigerC provides several commands to interact with the tigerS server:
*  tconnect 
*  rconnect (for debugging purposes)
*  whoami (for debugging)
*  tget
*  tput
*  tclose
*  help
*  quit 

Each command supports the 'help' parameter. Learn more by running 
tigerC and issuing 

```bash
<command> help
```

## Testing
Tiger-FTP supports many concurrent clients. To test this functionality:

1. run `./dupe.sh` to duplicate the 3mb.bin and 5mb.bin files. These files contain
random bytes up to their namesake file sizes. The files will be copied to the
./client/ and ./server/ directories.

2. change to the directory containing the built tigerS binary and start it:
    
```bash
./tigerS
```

The makefile should put tigerS in ./server/

3. Move test.sh to the client directory, if it's not already there, and change
directory to the client directory. This should be ./client/

4. From ./client/ run 

```bash
./test.sh
```

to demonstrate the functionality of the concurrent threads.

 .
..:

