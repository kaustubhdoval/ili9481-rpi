CC = gcc
CFLAGS = -Wall -Wextra -O0

SRCS = main.c ili9481_parallel.c ili9481_bmp.c
OBJS = $(SRCS:.c=.o)

main: $(OBJS)
	$(CC) $(OBJS) -o main 

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) main