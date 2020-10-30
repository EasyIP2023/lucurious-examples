CC=cc --std=gnu18 -Wall -Wextra -Werror -g -ggdb
LUCURIOUS_FLAGS=$(shell pkg-config lucurious --cflags --libs)

PIC="$(shell pwd)/ahsoka-tano-wallpaper.png"

all: se

se: simple_example.c
	$(CC) -D'IMG_SRC=$(PIC)' $(LUCURIOUS_FLAGS) -lm -o $@ $^

.PHONY: clean
clean:
	$(RM) se
