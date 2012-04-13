CC=gcc
CFLAGS=-Wall
STRIP=strip
STRIP_FLAGS=

all: cparse

cparse: cparse.c
	$(CC) $(CFLAGS) -o cparse cparse.c
	$(STRIP) $(STRIP_FLAGS) cparse

clean:
	rm -f *.o a.out *.core cparse
