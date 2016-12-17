# Use the GNU C/C++ compiler:
CC = gcc
CPP = g++

# COMPILER OPTIONS:

#OBJECT FILES
OBJS = dsh.c


dsh: dsh.c
	${CPP} ${OBJS} -o dsh -pthread
