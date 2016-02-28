INSTALL=/usr/bin/ginstall -c
CC=gcc
PREFIX=/usr/local
INCLUDE=/usr/X11R7/include
LD_PATH=/usr/X11R7/lib
RM=/bin/rm
all:
	${CC} main.c  -I${INCLUDE} -L${LD_PATH} -lX11 -lXpm -lpthread -o ybattery
clean:
	${RM} ybattery
install:
	${INSTALL} ybattery ${PREFIX}/bin
