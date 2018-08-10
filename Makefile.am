SUBDIRS = src
sysconf_DATA = swapspace.conf
EXTRA_DIST = swapspace.conf swapspace.lsbinit swapspace.service doc/

install-data-local:
	$(MKDIR_P) -m 700 $(DESTDIR)/$(localstatedir)/lib/swapspace
