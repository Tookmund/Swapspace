#! /usr/bin/make

SWAPPARENT=/var/lib
SWAPDIR=$(SWAPPARENT)/swapspace

all: VERSION DATE
	+$(MAKE) -C src VERSION="`cat VERSION`" DATE="`cat DATE`"
	+$(MAKE) -C doc VERSION="`cat VERSION`" DATE="`cat DATE`"

clean:
	+$(MAKE) -C src clean

distclean: clean
	+$(MAKE) -C src distclean

install: all
	install -d $(DESTDIR)/$(SWAPPARENT)
	install -d -m700 $(DESTDIR)/$(SWAPDIR)
	install -d $(DESTDIR)/usr/sbin $(DESTDIR)/etc $(DESTDIR)/etc/init.d
	install -d $(DESTDIR)/usr/share/man/man8
	strip src/swapspace || true
	install -m755 src/swapspace $(DESTDIR)/usr/sbin
	install -m644 swapspace.conf $(DESTDIR)/etc
	install -m744 debian/swapspace.init $(DESTDIR)/etc/init.d/swapspace
	install -m644 doc/swapspace.8 $(DESTDIR)/usr/share/man/man8

uninstall:
	/usr/sbin/swapspace -e
	$(RM) -r $(SWAPDIR)
	$(RM) /usr/sbin/swapspace /etc/swapspace.conf /etc/init.d/swapspace
	$(RM) /usr/share/man/man8/swapspace.8

dist : distclean VERSION
	tar -czf swapspace-`cat VERSION`.tar.gz --exclude=debian --exclude=.svn --exclude=.*.swp --exclude=tmp --exclude=swapspace-*.tar.gz .
	$(RM) -r tmp
	mkdir -p tmp/swapspace-`cat VERSION`
	tar -xzf swapspace-`cat VERSION`.tar.gz -C tmp/swapspace-`cat VERSION`
	$(RM) swapspace-`cat VERSION`.tar.gz
	tar -czf ../swapspace-`cat VERSION`.tar.gz -C tmp swapspace-`cat VERSION`
	$(RM) -r tmp


# Derive version number and last change date from Debian changelog
VERSION: debian/changelog
	head -n 1 $< | sed -e 's/^.*(\([0-9][^)]*\)).*/\1/' >$@

DATE: debian/changelog
	grep '^ -- ' $< | sed -e 's/^.*>  \([MTWFS][a-z]\{2\}, \{1,2\}[[:digit:]]\{1,2\} [JFBAMSOND][a-z]* 2[[:digit:]]\{3\}\) .*/\1/' | head -n 1 >$@

.PHONY: all clean install uninstall

