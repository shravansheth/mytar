CC = gcc
CFLAGS = -g -Wall
TARGET = mytar
SRCS = mytar.c writing.c listing.c extracting.c
HDRS = mytar.h writing.h listing.h extracting.h
OBJS = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(TARGET)

%.o: %.c $(HDRS)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)
