CC=gcc
CFLAGS=-Wall
DEBUG_CFLAGS=-DDEBUG=1
STRIP=strip
STRIP_FLAGS=

all: cparse

cparse: cparse.c
	$(CC) $(CFLAGS) -o cparse cparse.c
	$(STRIP) $(STRIP_FLAGS) cparse

cparse-debug: cparse.c
	$(CC) $(CFLAGS) $(DEBUG_CFLAGS) -o cparse-debug cparse.c
	$(STRIP) $(STRIP_FLAGS) cparse-debug

clean:
	rm -f *.o a.out *.core cparse cparse-debug
