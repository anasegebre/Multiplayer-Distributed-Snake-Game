CC := clang
CFLAGS := -g -Wall -Wno-deprecated-declarations -Werror

all: snake

clean:
	rm -rf snake snake.dSYM

snake: snake.c util.c util.h scheduler.c scheduler.h
	$(CC) $(CFLAGS) -o snake snake.c util.c scheduler.c -lncurses -lpthread
