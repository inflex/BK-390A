# 
# VERSION CHANGES
#

CFLAGS=-O
LIBS=
WINLIBS=-lgdi32 -lcomdlg32 -lcomctl32 -lmingw32
CC=gcc
WINCC=g++
# -fpermissive is needed to stop the warnings about casting stoppping the build
# -municode eliminates the WinMain@16 link error when we're using wWinMain
WINFLAGS=-fpermissive -static-libgcc -municode -fpermissive -static-libstdc++

OBJ=bk390a.exe
WINOBJ=win-bk390a.exe

default: 
	@echo
	@echo "   For OBS command line tool: make bk390a"
	@echo "   For GUI tool: make win-bk390a"
	@echo

win-bk390a: win-bk390a.cpp 
	${WINCC} ${CFLAGS} ${WINFLAGS} $(COMPONENTS) win-bk390a.cpp ${OFILES} -o ${WINOBJ} ${LIBS} ${WINLIBS}

bk390a: bk390a.c 
	${CC} ${CFLAGS} $(COMPONENTS) bk390a.c ${OFILES} -o ${OBJ} ${LIBS}

clean:
	del /s ${OBJ} ${WINOBJ}
