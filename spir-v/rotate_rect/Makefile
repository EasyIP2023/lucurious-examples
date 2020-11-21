CUR_DIR=$(shell pwd)

WAYLAND_PROTOS_DIR=$(shell pkg-config wayland-protocols --variable=pkgdatadir)
WAYLAND_SCANNER=$(shell pkg-config --variable=wayland_scanner wayland-scanner)
XDG_SHELL_PROTO=$(WAYLAND_PROTOS_DIR)/stable/xdg-shell/xdg-shell.xml
XDG_SHELL_FILES=xdg-shell-client-protocol.h xdg-shell-protocol.c

WAYLAND_FLAGS=$(shell pkg-config wayland-client --cflags)
WAYLAND_LIBS=$(shell pkg-config wayland-client --libs)
LUCURIOUS_LIBS=$(shell pkg-config lucurious --libs)

VERT=$(CUR_DIR)/vert.spv
FRAG=$(CUR_DIR)/frag.spv

CC=gcc
PROG=se
SPIRV=$(VERT) $(FRAG)
OBJS=simple_example.o
WAYLAND_OBJS=client.o xdg-shell-protocol.o
# common flags
COM_FLAGS=-Wall -Wextra -Werror -std=gnu18 -g -ggdb
CFLAGS=$(COM_FLAGS)
CFLAGS+=-DVERT_SHADER='"$(VERT)"' -DFRAG_SHADER='"$(FRAG)"'

LIBS=$(LUCURIOUS_LIBS) $(WAYLAND_LIBS)

all: $(SPIRV) $(XDG_SHELL_FILES) $(PROG)

$(PROG): $(WAYLAND_OBJS) $(OBJS)
	$(CC) $(CFLAGS) $^ $(LIBS) -o $@

$(WAYLAND_OBJS): %.o: %.c
	$(CC) -c $(COM_FLAGS) $(WAYLAND_FLAGS) $< $(WAYLAND_LIBS) -o $@

$(VERT):
	glslangValidator -V $(CUR_DIR)/shaders/shader.vert

$(FRAG):
	glslangValidator -V $(CUR_DIR)/shaders/shader.frag

xdg-shell-client-protocol.h:
	$(WAYLAND_SCANNER) client-header $(XDG_SHELL_PROTO) xdg-shell-client-protocol.h

xdg-shell-protocol.c:
	$(WAYLAND_SCANNER) private-code $(XDG_SHELL_PROTO) xdg-shell-protocol.c

.PHONY: clean
clean:
	$(RM) $(PROG) $(XDG_SHELL_FILES) *.o *.spv
