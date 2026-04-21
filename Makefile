CC      = gcc
CFLAGS  = -O2 -Wall -Wextra -std=c99 -Iinclude
TARGET  = sfs
SRCS    = src/main.c src/lzss.c src/crc32.c
OBJS    = $(SRCS:.c=.o)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

install: $(TARGET)
	install -m 755 $(TARGET) /usr/local/bin/sfs

uninstall:
	rm -f /usr/local/bin/sfs

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: install uninstall clean
