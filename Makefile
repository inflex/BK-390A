# 
# VERSION CHANGES
#

LOCATION=/usr/local
CFLAGS=-O
LIBS=
WINLIBS=-lgdi32 -lcomdlg32 -lcomctl32 -lmingw32
WINCC=i686-w64-mingw32-g++
# -fpermissive is needed to stop the warnings about casting stoppping the build
# -municode eliminates the WinMain@16 link error when we're using wWinMain
WINFLAGS=-fpermissive -municode -static-libgcc -fpermissive -static-libstdc++

OBJ=bk390a
WINOBJ=win-bk390a.exe
OFILES=

default: 
	@echo
	@echo "   For OBS command line tool: make bk390a"
	@echo "   For GUI tool: make win-bk390a"
	@echo

.c.o:
	${CC} ${CFLAGS} $(COMPONENTS) -c $*.c

all: ${OBJ} 

win-bk390a: ${OFILES} win-bk390a.cpp 
#	ctags *.[ch]
#	clear
	${WINCC} ${CFLAGS} ${WINFLAGS} $(COMPONENTS) win-bk390a.cpp ${OFILES} -o win-bk390a.exe ${LIBS} ${WINLIBS}

bk390a: ${OFILES} bk390a.c 
#	ctags *.[ch]
#	clear
	${CC} ${CFLAGS} $(COMPONENTS) bk390a.c ${OFILES} -o bk390a.exe ${LIBS}

strip: 
	strip *.exe

install: ${OBJ}
	cp bk390a win-bk390a ${LOCATION}/bin/

clean:
	rm -f *.o *core ${OBJ} ${WINOBJ}
