CC = gcc
CFLAGS = -Wall -Wcast-align -Wcast-qual -Wimplicit -Wpointer-arith -Wredundant-decls -Wreturn-type -Wshadow
PREFIX = /usr/local

all: gethostbyname gethostbyname2

gethostbyname: gethostbyname.c
	$(CC) $(CFLAGS) -o $@ $<

gethostbyname2: gethostbyname
	ln -sf $< $@

.PHONY: clean
clean:
	rm -f gethostbyname gethostbyname2

.PHONY: install
install: gethostbyname
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	install $^ $(DESTDIR)$(PREFIX)/bin/
	ln -sf $^ $(DESTDIR)$(PREFIX)/bin/gethostbyname2

.PHONY: uninstall
uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/gethostbyname
	rm -f $(DESTDIR)$(PREFIX)/bin/gethostbyname2
