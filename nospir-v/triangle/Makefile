
WAYLAND_PROTOS_DIR=$(shell pkg-config wayland-protocols --variable=pkgdatadir)
WAYLAND_SCANNER=$(shell pkg-config --variable=wayland_scanner wayland-scanner)
XDG_SHELL_PROTO=$(WAYLAND_PROTOS_DIR)/stable/xdg-shell/xdg-shell.xml
XDG_SHELL_FILES=xdg-shell-client-protocol.h xdg-shell-protocol.c

WAYLAND_FLAGS=$(shell pkg-config wayland-client --cflags)
WAYLAND_LIBS=$(shell pkg-config wayland-client --libs)
LUCURIOUS_LIBS=$(shell pkg-config lucurious --libs)

CC=gcc
PROG=se
OBJS=simple_example.o
WAYLAND_OBJS=client.o xdg-shell-protocol.o
CFLAGS=-Wall -Wextra -Werror -std=gnu18 -g -ggdb

LIBS=$(LUCURIOUS_LIBS) $(WAYLAND_LIBS)

all: $(XDG_SHELL_FILES) $(PROG)

$(PROG): $(WAYLAND_OBJS) $(OBJS) 
	$(CC) $(CFLAGS) $^ $(LIBS) -o $@

$(WAYLAND_OBJS): %.o: %.c
	$(CC) -c $(CFLAGS) $(WAYLAND_FLAGS) $< $(WAYLAND_LIBS) -o $@

xdg-shell-client-protocol.h:
	$(WAYLAND_SCANNER) client-header $(XDG_SHELL_PROTO) xdg-shell-client-protocol.h

xdg-shell-protocol.c:
	$(WAYLAND_SCANNER) private-code $(XDG_SHELL_PROTO) xdg-shell-protocol.c

.PHONY: clean
clean:
	$(RM) $(PROG) $(XDG_SHELL_FILES) *.o
