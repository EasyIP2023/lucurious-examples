CC=cc --std=gnu18 -Wall -Wextra -Werror -g
LUCURIOUS_FLAGS=$(shell pkg-config lucurious --cflags --libs)

all: se

se: simple_example.c
	$(CC) $(LUCURIOUS_FLAGS) -o $@ $^

.PHONY: clean
clean:
	$(RM) se
