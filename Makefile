CC = gcc
CFLAGS = -Wall -O2

all: teensy_size

teensy_size: teensy_size.o minimal_elf.o
	$(CC) -o $@ $^

clean:
	rm -f *.o teensy_size
