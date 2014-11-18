#! /usr/bin/make

# Assumes we're using gcc
# swapspace is written in C99, but with at least one extension that isn't
# supported in gcc's C99 mode: sigaction() (though it is part of POSIX).
CFLAGS += --std=gnu99 -Wall --pedantic -Wshadow -O2 -g
CPPFLAGS += -DSUPPORT_LARGE_FILES -DVERSION="\"$(VERSION)\"" -DDATE="\"$(DATE)\""
RM=rm -f

# Uncomment this to build a special ultra-lightweight unconfigurable version of
# swapspace.  This saves perhaps 20 kilobytes of binary size and breaks all but
# a few of the command line options.  You probably don't want this; it's mostly
# here as a proof-of-concept and testing ground for the future elimination of
# configuration items.
#CPPFLAGS += -DNO_CONFIG

# Uncomment this to disable assertions and internal consistency checking that
# you probably don't need after release.
# CPPFLAGS += -DNDEBUG

all : swapspace hog


SWAPSPACEOBJS=log.o main.o memory.o opts.o state.o support.o swaps.o

swapspace : $(SWAPSPACEOBJS)
	$(CC) $^ $(LDFLAGS) $(LOADLIBES) $(LDLIBS) -o $@

hog : hog.o

log.o : log.c log.h main.h memory.h

main.o : main.c config.h env.h log.h main.h memory.h support.h

memory.o : memory.c config.h env.h log.h main.h memory.h support.h

opts.o : opts.c opts.h main.h ../VERSION ../DATE

state.o : state.c state.h log.h main.h memory.h support.h swaps.h

support.o : support.c config.h env.h support.h

swaps.o : swaps.c config.h env.h log.h main.h memory.h state.h support.h swaps.h

clean :
	$(RM) $(SWAPSPACEOBJS) hog.o

distclean : clean
	$(RM) swapspace hog

.PHONY : all clean distclean

