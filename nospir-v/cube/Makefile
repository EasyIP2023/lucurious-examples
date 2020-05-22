
WAYLAND_FLAGS=$(shell pkg-config wayland-client --cflags --libs)
WAYLAND_PROTOS_DIR=$(shell pkg-config wayland-protocols --variable=pkgdatadir)
WAYLAND_SCANNER=$(shell pkg-config --variable=wayland_scanner wayland-scanner)

XDG_SHELL_PROTO=$(WAYLAND_PROTOS_DIR)/stable/xdg-shell/xdg-shell.xml
XDG_SHELL_FILES=xdg-shell-client-protocol.h xdg-shell-protocol.c

CC=cc --std=gnu18 -Wall -Wextra -Werror -g
LUCURIOUS_FLAGS=$(shell pkg-config lucurious --cflags --libs)

all: se

se: simple_example.c client.c $(XDG_SHELL_FILES)
	$(CC) $(LUCURIOUS_FLAGS) $(WAYLAND_FLAGS) -o $@ $^

xdg-shell-client-protocol.h:
	$(WAYLAND_SCANNER) client-header $(XDG_SHELL_PROTO) xdg-shell-client-protocol.h

xdg-shell-protocol.c:
	$(WAYLAND_SCANNER) private-code $(XDG_SHELL_PROTO) xdg-shell-protocol.c

.PHONY: clean
clean:
	$(RM) se $(XDG_SHELL_FILES)
