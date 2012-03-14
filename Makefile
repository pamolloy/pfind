CC=gcc
CFLAGS=-Wall -g
NAME=pfind

$(NAME): main.c
	$(CC) $(CFLAGS) -o $(NAME) main.c

.PHONY: clean
clean:
	rm -f $(NAME)
