![logo](doc/logo.svg)

Swapspace - dynamic swap manager for Linux
==========================================

**N.B. If you want suspend and resume to work you will still need a static
swapfile!**

Where to start
--------------
If you cloned this repo be sure to run `autoreconf` to make the configure script.
If you downloaded a tarball from github the configure script should be there
already.
If you install to somewhere other than ```/usr/local/``` be sure to edit the
```ETCPREFIX``` and ```VARPREFIX``` variables in ```src/env.h``` as well as the
initscript.

For detailed installation instructions, see `INSTALL`

Be sure to install the relevant initscript as well to have this run at startup.
Swapspace ships a systemd unit file as `swapspace.service` that should be
installed in `/etc/systemd/system/` or a SysVinit script as `swapspace.lsbinit`
that should be installed in `/etc/init.d/` as just `swapspace`.

What it does for you
--------------------

This is a system daemon for the Linux kernel that eliminates the need for large,
fixed swap partitions.

Usually when you install a GNU/Linux system, it sets up a swap partition on
disk.  The swap partition serves as virtual memory, so you may need a lot of it.
But you can't store data there, so you don't want to sacrifice too much disk
space.  And it's not always easy, or safe, to change its size afterwards!

Running swapspace solves that problem.  You no longer need a large swap
partition.  You can even do without the whole thing.  The program manages swap
files for you.  These work just like partitions, except they're normal files.
You can add more when you need them, or delete some when you want the disk space
back.  And that is exactly what swapspace does.  It constantly monitors your
system's virtual-memory needs and manages a pool of swap files accordingly.

With swapspace you can install GNU/Linux in one single big partition, without
regrets later about picking the wrong size.  Your system can handle the
occasional memory-intensive task, without eating up disk space that you'll never
get back.

Swapspace is made available for use under the GNU General Public License (GPL).
See the file COPYING for an open copyright license.

Copyright (C) 2005, 2006 Software Industry Promotion Agency (SIPA), Thailand.

Copyright (C) 2010, 2012 Jeroen T. Vermeulen.

Written by Jeroen T. Vermeulen.

Maintained by Jacob Adams.

When not to use it
------------------

In its current form, swapspace is probably not a good choice for systems that
need to remain responsive at all times.  Depending on the system and the
circumstances, the creation of a large new swapfile can take as long as half a
minute and occupy quite a lot of the system's attention.  The program minimizes
the impact, but it wouldn't be very useful if it never created any swapfiles at
all!

The slowdown happens in operating-system code, so there isn't much we can do
about it.  It would help if the kernel were able to extend a swap file that's
already in use.  But that is of course the price you pay for virtual memory:
slower but cheaper.  If your system is too slow because there's not enough
memory, the ultimate answer is still: try to use less or buy more!

See also
--------
Home page: https://github.com/Tookmund/swapspace

Old Home page: http://pqxx.org/development/swapspace/

You'll find a manual under "doc" in the source tree.  Read it with:

    man -l doc/swapspace.8

For technical details of the installation and the algorithm, see `doc/technicaldetails.md`

For a comparison with other swapspace managers, see `doc/comparison.md`
