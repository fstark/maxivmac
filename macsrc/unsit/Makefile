CC = gcc
CFLAGS = -Wall -Wextra -O2
OBJS = unsit.o core.o sit5.o sit1.o arsenic.o lzss13.o compress.o \
       huffman.o bitreader.o crc.o bwt.o

unsit: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

%.o: %.c unsit.h
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f unsit $(OBJS)
