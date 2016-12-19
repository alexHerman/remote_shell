# Use the GNU C/C++ compiler:
CC = gcc
CPP = g++

# COMPILER OPTIONS:

#OBJECT FILES
OBJS = dsh.c

all: server client dsh

server: shell_server.cpp
	${CPP} shell_server.cpp -o shellServer

client: shell_client.cpp
	${CPP} shell_client.cpp -o shellClient

dsh: dsh.c
	${CPP} ${OBJS} -o dsh -pthread

clean:
	rm dsh shellClient shellServer
