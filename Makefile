CFLAGS?=	-Wall -ggdb -W -O2
CC?=		gcc
LIBS?=
LDFLAGS?=	-lpthread
PREFIX?=	/usr
VERSION=	0.1
TMPDIR=/tmp/webbench_pro-$(VERSION)

all:   webbench_pro tags

tags:  *.c
	-ctags *.c

install: webbench_pro
	install -s webbench_pro $(DESTDIR)$(PREFIX)/bin	
	install -m 644 webbench_pro.1 $(DESTDIR)$(PREFIX)/share/man/man1	
	install -d $(DESTDIR)$(PREFIX)/share/doc/webbench_pro
	install -m 644 README $(DESTDIR)$(PREFIX)/share/doc/webbench_pro
	install -m 644 Changelog $(DESTDIR)$(PREFIX)/share/doc/webbench_pro

webbench_pro: webbench_pro.o socket.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o webbench_pro *.o $(LIBS) 

clean:
	-rm -f *.o webbench_pro *~ core *.core tags
	
tar:   clean
	-debian/rules clean
	rm -rf $(TMPDIR)
	install -d $(TMPDIR)
	cp -p Makefile webbench_pro.c socket.c webbench_pro.1 $(TMPDIR)
	ln -sf  README $(TMPDIR)/README
	ln -sf Changelog $(TMPDIR)/ChangeLog
	-cd $(TMPDIR) && cd .. && tar cozf webbench_pro-$(VERSION).tar.gz webbench_pro-$(VERSION)

webbench_pro.o:	webbench_pro.c
socket.o:	socket.c

.PHONY: clean install all tar
