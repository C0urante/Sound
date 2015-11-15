CC = clang
CFLAGS = -std=c99 -Wall -Wextra

all: sound

sound: sound.c
	$(CC) $(CFLAGS) -lm -o sound sound.c

clean:
	rm -f sound
