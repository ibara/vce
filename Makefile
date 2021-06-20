# vce Makefile

PROG =	vce
OBJS =	vce.o

CC =		cc
CFLAGS =	-g -O2 -DANSI

PREFIX ?=	/usr/local

all: unix

unix: ${OBJS}
	${CC} ${LDFLAGS} -o ${PROG} ${OBJS}

cpm:
	ack -mcpm -O6 -DANSI -D__cpm__ -o vce.com vce.c

msdos:
	wcl -0 -ox -mt -DANSI -D__msdos__ vce.c

install:
	install -m 755 ${PROG} ${PREFIX}/bin

clean:
	rm -f ${PROG} ${OBJS} vce.com vce.core
