CC=gcc
CFLAGS=-Wall -Wextra -std=c99 -pedantic -Wno-sign-compare
TARGET=editor

all: $(TARGET)

$(TARGET): editor.c
	$(CC) $(CFLAGS) -o $(TARGET) editor.c

clean:
	rm -f $(TARGET)

install: $(TARGET)
	cp $(TARGET) /usr/local/bin/

.PHONY: all clean install
