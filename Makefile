CC = clang
CFLAGS = -Wall -Wextra -pthread
TARGET = iching 
SRCS = main.c
OBJS = $(SRCS:.c=.o)

.PHONY: all install clean

all: $(TARGET) install clean

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(TARGET) $(OBJS)

install:
	mv $(TARGET) $(HOME)/.local/bin
