# 
# VERSION CHANGES
#

LOCATION=/usr/local
CFLAGS=-Wall -O2 
LIBS=

OBJ=bk390a
OFILES=
default: bk390a

.c.o:
	${CC} ${CFLAGS} $(COMPONENTS) -c $*.c

all: ${OBJ} 

bk390a: ${OFILES} bk390a.c 
#	ctags *.[ch]
#	clear
	${CC} ${CFLAGS} $(COMPONENTS) bk390a.c ${OFILES} -o bk390a ${LIBS}

install: ${OBJ}
	cp bk390a ${LOCATION}/bin/

clean:
	rm -f *.o *core ${OBJ}
