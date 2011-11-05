CFLAGS?=	-Wall -ggdb -W -O2
CC?=		gcc
LIBS?=
LDFLAGS?=	-lpthread
PREFIX?=	/usr/local
VERSION=	0.1
TMPDIR=/tmp/webbench-$(VERSION)

all:   webbench tags

tags:  *.c
	-ctags *.c

install: webbench
	install -s webbench $(DESTDIR)$(PREFIX)/bin	
	install -m 644 webbench.1 $(DESTDIR)$(PREFIX)/man/man1	
	install -d $(DESTDIR)$(PREFIX)/share/doc/webbench
	install -m 644 debian/copyright $(DESTDIR)$(PREFIX)/share/doc/webbench
	install -m 644 debian/changelog $(DESTDIR)$(PREFIX)/share/doc/webbench

webbench: webbench.o socket.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o webbench *.o $(LIBS) 

clean:
	-rm -f *.o webbench *~ core *.core tags
	
tar:   clean
	-debian/rules clean
	rm -rf $(TMPDIR)
	install -d $(TMPDIR)
	cp -p Makefile webbench.c socket.c webbench.1 $(TMPDIR)
	install -d $(TMPDIR)/debian
	-cp -p debian/* $(TMPDIR)/debian
	ln -sf debian/copyright $(TMPDIR)/COPYRIGHT
	ln -sf debian/changelog $(TMPDIR)/ChangeLog
	-cd $(TMPDIR) && cd .. && tar cozf webbench-$(VERSION).tar.gz webbench-$(VERSION)

webbench.o:	webbench.c
socket.o:	socket.c

.PHONY: clean install all tar
