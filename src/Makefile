CFLAGS  = -std=gnu99 -Wall -Werror -Wno-unused -pthread -D_GNU_SOURCE -lpcre2-8 -I./include/ -L./lib/ -lfiredns

CC = gcc

TARGET = smtp
SRCS = main.c commands.c util.c smtp.c dynamic_vector.c logger.c smtp_sockets.c maildir.c message_builder.c serverstate-fsm.c smtp_time.c server_replies.c smtp_worker.c smtp_worker_actions.c
OBJS = $(SRCS:.c=.o)

.PHONY: all clean

all: $(TARGET)
$(TARGET): $(OBJS)
	$(CC) -o $(TARGET) $(OBJS) $(CFLAGS)
 
.c.o:
	$(CC) $(CFLAGS)  -c $< -o $@

clean:
	rm -rf $(TARGET) $(OBJS) *.txt
