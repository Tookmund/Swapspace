/*
This file is part of Swapspace.

Copyright (C) 2005,2006, Software Industry Industry Promotion Agency (SIPA)
Written by Jeroen T. Vermeulen <jtv@xs4all.nl>.

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
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>

// Memory hog program.  Allocates amount requested on the command line (in
// megabytes) and keeps it, or if no argument is given, reads standard input and
// tries to follow the amounts requested there.

static void usage(const char name[])
{
  fprintf(stderr, "Usage: %s [megabytes]\n", name);
}


void gimme(unsigned long megs)
{
  static char *ptr = NULL;
  static size_t oldsize = 0;
  
  const size_t bytes = megs*1024*1024;

  printf("Allocating %lu MB: ", megs);
  fflush(NULL);
  ptr = realloc(ptr, bytes);
  if (!ptr)
  {
    printf("Failed!\n");
    exit(EXIT_FAILURE);
  }
  if (bytes > oldsize) memset(ptr+oldsize, '#', bytes-oldsize);
  oldsize = bytes;
  printf("0x%lx bytes\n", (unsigned long)bytes);
}


static int loop(void)
{
  char buf[1000];
  while (fgets(buf, sizeof(buf), stdin))
  {
    const size_t target = strtoul(buf, NULL, 0);
    if (target) gimme(target);
  }
  return EXIT_SUCCESS;
}

int main(int argc, char *argv[])
{
  unsigned long target;
  switch (argc)
  {
  case 1:
    puts("Just keep entering how much memory you'd like, in megabytes:");
    return loop();
  case 2:
    target = strtoul(argv[1], NULL, 0);
    if (!target)
    {
      usage(argv[0]);
      return EXIT_FAILURE;
    }
    gimme(target);
    sleep(3600);
    puts("Timing out");
    return EXIT_SUCCESS;
  default:
    usage(argv[0]);
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}

