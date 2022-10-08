# Makefile for writer.c
# Author: Guruprashanth Krishnakumar, gukr5411@colorado.edu
# Date: 09/04/2022
INCLUDES ?= 
LIB_DIRS = 
# CROSS_COMPILE variable initially set to nothing. Will be over-written if value is passed in the command-line.
CROSS_COMPILE =
CC ?= $(CROSS_COMPILE)gcc
TARGET ?= aesdsocket

# Compile at O0 by default
CFLAGS ?= -O0 -g -Wall -Werror 
LDFLAGS ?= -pthread -lrt

HFILES =queue.h
CFILES = $(TARGET).c

SRCS = ${HFILES} ${CFILES}
OBJS = ${CFILES:.c=.o}

all:	$(TARGET)

# Remove all .o files and the writer executable
clean:
	-rm -f *.o *.d
	-rm -f $(TARGET)

# native build if no args are passed, build for the specified target in case args are passed.
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(INCLUDES) $(LDFLAGS)

depend:

.c.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c $<
