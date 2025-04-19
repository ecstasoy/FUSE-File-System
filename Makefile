#
# file:        Makefile
#

CFLAGS = -ggdb3 -Wall -O0 -I/opt/homebrew/include
LDLIBS = -L/opt/homebrew/lib -lcheck -lz -lm -lpthread -lfuse

all: unittest-1 unittest-2 fuse test.img test2.img

unittest-1: test/unittest-1.o src/filesystem.o src/misc.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

unittest-2: test/unittest-2.o src/filesystem.o src/misc.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

fuse: src/misc.o src/filesystem.o src/fuse.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)


# force test.img, test2.img to be rebuilt each time
.PHONY: test.img test2.img

test.img: 
	python gen-disk.py -q disk1.in test.img

test2.img: 
	python gen-disk.py -q disk2.in test2.img

clean: 
	rm -f *.o unittest-1 unittest-2 fuse test.img test2.img diskfmt.pyc

test/%.o: test/%.c
	$(CC) $(CFLAGS) -c $< -o $@

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@