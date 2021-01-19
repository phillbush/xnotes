PROG = xcount
SRCS = ${PROG}.c
OBJS = ${SRCS:.c=.o}

INCS = -I/usr/X11R6/include
LIBS = -L/usr/X11R6/lib -lX11

CC = cc
CFLAGS = -g -O0 -Wall -Wextra ${INCS}
LDFLAGS = ${LIBS}

all: ${PROG}

${PROG}: ${OBJS}
	${CC} -o $@ ${OBJS} ${LDFLAGS}

.c.o:
	${CC} ${CFLAGS} -c $<

clean:
	-rm ${OBJS} ${PROG}

test: ${PROG}
	./${PROG}

.PHONY: all clean test
