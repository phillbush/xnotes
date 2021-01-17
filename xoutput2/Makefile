PROG = xoutput
SRCS = ${PROG}.c
OBJS = ${SRCS:.c=.o}

INCS = -I/usr/X11R6/include -I/usr/include/freetype2 -I/usr/X11R6/include/freetype2
LIBS = -L/usr/X11R6/lib -lfontconfig -lXft -lX11

CC = cc
CFLAGS = -Wall -Wextra ${INCS}
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
