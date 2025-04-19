#
# file:        Makefile
#

CFLAGS = -ggdb3 -Wall -O0 -I/opt/homebrew/include
LDLIBS = -L/opt/homebrew/lib -lcheck -lz -lm -lpthread -lfuse

all: unittest-1 unittest-2 fuse test.img test2.img

unittest-1: unittest-1.o filesystem.o misc.o

unittest-2: unittest-2.o filesystem.o misc.o

fuse: misc.o filesystem.o fuse.o


# force test.img, test2.img to be rebuilt each time
.PHONY: test.img test2.img

test.img: 
	python gen-disk.py -q disk1.in test.img

test2.img: 
	python gen-disk.py -q disk2.in test2.img

clean: 
	rm -f *.o unittest-1 unittest-2 fuse test.img test2.img diskfmt.pyc
