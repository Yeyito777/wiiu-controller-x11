CC ?= cc
CFLAGS ?= -Wall -Wextra -O2
PREFIX ?= $(HOME)/.local

.PHONY: all install clean

all: xwiictl wiictl

xwiictl: xwiictl.c
	$(CC) $(CFLAGS) -o $@ $< $$(pkg-config --cflags --libs x11)

wiictl: wiictl.c
	$(CC) $(CFLAGS) -o $@ $<

install: all
	mkdir -p $(PREFIX)/bin
	cp -f xwiictl wiictl wiiu-controller $(PREFIX)/bin/

clean:
	rm -f xwiictl wiictl
