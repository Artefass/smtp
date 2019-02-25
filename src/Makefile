CFLAGS  = -std=gnu99 -Wall -Werror -pthread

CC = gcc

TARGET = smtp
SRCS = main.c commands.c smtp.c dynamic_vector.c logger.c smtp_sockets.c
OBJS = $(SRCS:.c=.o)

.PHONY: all clean

all: $(TARGET)
$(TARGET): $(OBJS)
	$(CC) -o $(TARGET) $(OBJS) $(CFLAGS)
 
.c.o:
	$(CC) $(CFLAGS)  -c $< -o $@

clean:
	rm -rf $(TARGET) $(OBJS) *.txt
