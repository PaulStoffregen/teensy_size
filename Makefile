CC = gcc
CFLAGS = -Wall -O2

all: teensy_size

teensy_size: teensy_size.o teensy_info.o elf.o
	$(CC) -o $@ $^

clean:
	rm -r *.o teensy_size
