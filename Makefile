CC = gcc
CFLAGS = -Wall -Wextra
LIBS = -lgpiod

SRCS = main.c ili9481_parallel.c  
OBJS = $(SRCS:.c=.o)

main: $(OBJS)
	$(CC) $(OBJS) -o main $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) main