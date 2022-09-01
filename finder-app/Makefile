INCLUDE_DIRS = 
LIB_DIRS = 
CROSS-COMPILE=
cc=gcc

CDEFS=
CFLAGS= -O0 -g $(INCLUDE_DIRS) $(CDEFS)
LIBS= 

HFILES= 
CFILES= writer.c

SRCS= ${HFILES} ${CFILES}
OBJS= ${CFILES:.c=.o}

all:	writer

clean:
	-rm -f *.o *.d
	-rm -f writer

writer: writer.o
	$(CROSS-COMPILE)$(cc) $(LDFLAGS) $(CFLAGS) -o $@ $@.o

depend:

.c.o:
	$(CROSS-COMPILE)$(cc) $(CFLAGS) -c $<
