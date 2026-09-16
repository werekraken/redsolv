CC = gcc
CFLAGS = -Wall -Wcast-align -Wcast-qual -Wimplicit -Wpointer-arith -Wredundant-decls -Wreturn-type -Wshadow
PREFIX = /usr/local

all: getaddrinfo gethostbyname gethostbyname2

getaddrinfo: getaddrinfo.c
	$(CC) $(CFLAGS) -o $@ $<

gethostbyname: gethostbyname.c
	$(CC) $(CFLAGS) -o $@ $<

gethostbyname2: gethostbyname
	ln -sf $< $@

.PHONY: clean
clean:
	rm -f getaddrinfo gethostbyname gethostbyname2

.PHONY: install
install: getaddrinfo gethostbyname
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	install $^ $(DESTDIR)$(PREFIX)/bin/
	ln -sf gethostbyname $(DESTDIR)$(PREFIX)/bin/gethostbyname2

.PHONY: uninstall
uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/getaddrinfo
	rm -f $(DESTDIR)$(PREFIX)/bin/gethostbyname
	rm -f $(DESTDIR)$(PREFIX)/bin/gethostbyname2
