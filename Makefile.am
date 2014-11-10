SUBDIRS = src
sysconf_DATA = swapspace.conf

install-data-local:
	$(MKDIR_P) $(localstatedir)/lib/swapspace
	
install-exec-hook:
	echo "Be sure to install appropriate initscript"
