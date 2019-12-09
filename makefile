#! /bin/bash
.PHONY:clean

default:
	gcc -O socket_server.c -o server -levent
	gcc -O socket_client.c -o client -levent

clean:
	rm server client 
