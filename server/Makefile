# Makefile for writer.c
# Author: Guruprashanth Krishnakumar, gukr5411@colorado.edu
# Date: 09/04/2022
INCLUDE_DIRS = 
LIB_DIRS = 
# CROSS_COMPILE variable initially set to nothing. Will be over-written if value is passed in the command-line.
CROSS_COMPILE=
CC=gcc

CDEFS=
# Compile at O0 by default
CFLAGS= -O0 -g -Wall -Werror $(INCLUDE_DIRS) $(CDEFS)
LIBS= -lpthread -lrt

HFILES=queue.h
CFILES= aesdsocket.c

SRCS= ${HFILES} ${CFILES}
OBJS= ${CFILES:.c=.o}

all:	aesdsocket

# Remove all .o files and the writer executable
clean:
	-rm -f *.o *.d
	-rm -f aesdsocket

# native build if no args are passed, build for the specified target in case args are passed.
aesdsocket: aesdsocket.o
	$(CROSS_COMPILE)$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $@.o $(LIBS)

depend:

.c.o:
	$(CROSS_COMPILE)$(CC) $(CFLAGS) -c $<
