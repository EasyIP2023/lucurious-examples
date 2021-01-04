LUCURIOUS_FLAGS=$(shell pkg-config lucurious --cflags)
LUCURIOUS_LIBS=$(shell pkg-config lucurious --libs)

CC=gcc
PROG=se
OBJS=simple_example.o
CFLAGS=-Wall -Wextra -Werror -std=gnu18 -g -ggdb $(LUCURIOUS_FLAGS)
LIBS=$(LUCURIOUS_LIBS) -lm

all: $(PROG)

$(PROG): $(OBJS)
	$(CC) $(CFLAGS) $^ $(LIBS) -o $@

.PHONY: clean
clean:
	$(RM) $(PROG) *.o
