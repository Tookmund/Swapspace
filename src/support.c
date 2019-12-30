/*
This file is part of Swapspace.

Copyright (C) 2005,2006, Software Industry Industry Promotion Agency (SIPA)
Copyright (C) 2010, Jeroen T. Vermeulen
Written by Jeroen T. Vermeulen.

Swapspace is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

Swapspace is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with swapspace; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
*/
#include "env.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "main.h"
#include "support.h"

#ifndef HAVE_SWAPON
/// Replacement function for system function.  Clobbers localbuf.
int swapon(const char path[], int flags)
{
  return runcommand("/sbin/swapon", path);
}
#endif

#ifndef HAVE_SWAPOFF
/// Replacement function for system function.  Clobbers localbuf.
int swapoff(const char path[])
{
  return runcommand("/sbin/swapoff", path);
}
#endif


int runcommandformat(const char format[], const char cmd[], const char arg[])
{
  const size_t bufsz = sizeof(localbuf);
  if (unlikely(snprintf(localbuf, bufsz, format, cmd, arg) >= bufsz))
  {
    errno = E2BIG;
    return -1;
  }
  return system(localbuf);
}

int runcommand(const char cmd[], const char arg[])
{
  return runcommandformat("%s '%s'", cmd, arg);
}
