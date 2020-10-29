CC = gcc
LIBS = -pthread
CFLAGS = -std=c99 -Wall -pedantic
SERVERDIR = ./server
CLIENTDIR = ./client

.PHONY: all server client help test
.SILENT: help tigerS tigerC

all: server client

help:
	echo "Usage options: all tigerS server tigerC client"

server: tigerS

client: tigerC

tigerS: tigerS.c 
	$(CC) $(CFLAGS) $(LIBS) $^ -o $@
	mkdir -p $(SERVERDIR)
	mv $@ $(SERVERDIR)
	cp ./authorized.txt $(SERVERDIR)

tigerC: tigerC.c 
	$(CC) $(CFLAGS) $^ -o $@
	mkdir -p $(CLIENTDIR)
	mv $@ $(CLIENTDIR)

clean:
	-rm -rf *.o tigerS tigerC
	-rm -rf $(CLIENTDIR)/*.bin $(SERVERDIR)/*.bin
