CC = gcc
CFLAGS = -Wall -Wextra -pedantic
LDFLAGS = -lraylib -lm

snake: main.c
	$(CC) $(CFLAGS) $(LDFLAGS) main.c -o snake

