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

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/param.h>

#include "memory.h"
#include "opts.h"
#include "support.h"
#include "state.h"
#include "swaps.h"


static const char copyright[] = "\n"
      "Copyright (c) 2004-2006 Software Industry Promotion Agency of Thailand\n"
      "Copyright (c) 2010 Jeroen T. Vermeulen\n"
      "Written by Jeroen T. Vermeulen\n"
      "This program is free software, and is available for use under the GNU\n"
      "General Public License (GPL).\n"
      "See http://pqxx.org/development/swapspace\n"
      "This is a fork of the original project by Jacob Adams hosted at https://github.com/Tookmund/swapspace\n";


#ifndef NO_CONFIG
static char configfile[PATH_MAX] = "/etc/swapspace.conf";

static char *set_configfile(long long dummy)
{
  return configfile;
}

static char *set_help(long long dummy);
static char *set_version(long long dummy);

static bool inspect = false;
static char *set_inspect(long long dummy)
{
  inspect = true;
  return NULL;
}

enum argtype { at_none, at_num, at_str };
struct option
{
  const char *name;
  char shortopt;
  enum argtype argtype;
  /// Minimum value for integer option, or for string, whether arg is required
  long long min;
  /// Maximum value for integer option, or for string, maximum length
  long long max;
  char *(*setter)(long long value);
  const char *desc;
};

/// Available options, sorted alphabetically by long option name
static const struct option options[] =
{
  { "buffer_elasticity",'B', at_num,  0, 100, set_buffer_elasticity,
  "Consider n% of buffer memory to be \"available\"" },
  { "cache_elasticity",	'C', at_num,  0, 100, set_cache_elasticity,
  "Consider n% of cache memory to be \"available\"" },
  { "configfile",	'c', at_str,  1, PATH_MAX, set_configfile,
  "Use configuration file s" },
  { "cooldown",		'a', at_num,  0, LONG_MAX, set_cooldown,
  "Give allocation attempts n seconds to settle" },
  { "daemon",		'd', at_none, 0, 0, set_daemon,
  "Run quietly in background" },
  { "erase",		'e', at_none, 0, 0, set_erase,
  "Try to free up all swapfiles, then exit" },
  { "freetarget", 	'f', at_num,  2, 99, set_freetarget,
  "Aim for n% of available space" },
  { "help",		'h', at_none, 0, 0, set_help,
  "Display usage information" },
  { "inspect",		'i', at_none, 0, 0, set_inspect,
  "Verify that configuration is okay, then exit" },
  { "lower_freelimit",	'l', at_num,  0, 99, set_lower_freelimit,
  "Try to keep at least n% of memory/swap available" },
  { "max_swapsize",	'M', at_num, 8192, LLONG_MAX, set_max_swapsize,
  "Restrict swapfiles to n bytes" },
  { "min_swapsize",	'm', at_num, 8192, LLONG_MAX, set_min_swapsize,
  "Don't create swapfiles smaller than n bytes" },
  { "paranoid",		'P', at_none, 0, 0, set_paranoid,
  "Wipe disk space occupied swapfiles after use" },
  { "pidfile",		'p', at_str,  0, PATH_MAX, set_pidfile,
  "Write process identifier to file s" },
  { "quiet",		'q', at_none, 0, 0, set_quiet,
  "Suppress informational output" },
  { "swappath",		's', at_str,  1, PATH_MAX, set_swappath,
  "Create swapfiles in secure directory s" },
  { "upper_freelimit",	'u', at_num,  0, 100, set_upper_freelimit,
  "Reduce swapspace if more than n% is free" },
  { "verbose",		'v', at_none, 0, 0, set_verbose,
  "Print lots of debug information" },
  { "version",		'V', at_none, 0, 0, set_version,
  "Print version number and exit" }
};


static int optcmp(const void *o1, const void *o2)
{
  return strcmp(((const struct option *)o1)->name,
                ((const struct option *)o2)->name);
}


#ifndef NDEBUG
static bool options_okay(void)
{
  static const int num_opts = sizeof(options)/sizeof(*options);
  for (int i=0; i<num_opts; ++i)
  {
    if ((unsigned char)options[i].shortopt >= 127)
    {
      fprintf(stderr, "Shortopt for option %d out of range\n", i);
      return false;
    }
    if (options[i].min > options[i].max)
    {
      fprintf(stderr,"Minimum for %s exceeds maximum\n", options[i].name);
      return false;
    }
    if (!options[i].setter)
    {
      fprintf(stderr,"No setter defined for option %s\n",options[i].name);
      return false;
    }
  }
  localbuf[(unsigned char)options[0].shortopt] = true;
  for (int i=1; i<num_opts; ++i)
  {
    if (strcmp(options[i-1].name,options[i].name) >= 0)
    {
      fprintf(stderr,
	  "Options array not sorted (%s >= %s)\n",
	  options[i-1].name,
	  options[i].name);
      return false;
    }
    if (localbuf[(unsigned char)options[i].shortopt])
    {
      fprintf(stderr,
	  "Option %s re-uses shortopt '%c'\n",
	  options[i].name,
	  options[i].shortopt);
      return false;
    }
  }
  return true;
}
#endif	// NDEBUG

static const char *argproto(int opt, bool shortopt)
{
  const char *result = "";
  switch (options[opt].argtype)
  {
  case at_none: break;
  case at_num: result = (shortopt ? " n" : "=n"); break;
  case at_str: result = (shortopt ? " s" : "=s"); break;
  }
  return result;
}

static char *set_help(long long dummy)
{
  puts("Usage: swapspace [options]\n"
      "Dynamic swap space manager for GNU/Linux.\n\n"
      "Options are:");

  size_t longestopt = 1;
  for (int i=0; i < sizeof(options)/sizeof(*options); ++i)
  {
    size_t len = strlen(options[i].name);
    if (len > longestopt) longestopt = len;
  }

  for (int i=0; i < sizeof(options)/sizeof(*options); ++i)
  {
    static const char pad[] = "   ";
    const char *const pt = argproto(i, true);
    size_t ptlen;
    
    ptlen = strlen(pt);
    assert(ptlen < strlen(pad));

    printf("  -%c%s,%s--%s%s%*s\t%s\n",
	options[i].shortopt,
	pt,
	pad+ptlen,
	options[i].name,
	argproto(i, false),
	(int)(longestopt+2-strlen(options[i].name)-2*strlen(pt)),
	"",
	options[i].desc);
  }

  puts(copyright);
  exit(EXIT_SUCCESS);
}
#else

static void short_usage(void)
{
  puts("This is the unconfigurable version of swapspace.\n"
      "\n"
      "Usage: swapspace [-d] [-h] [-p] [-V]\n"
      "\t-d run as daemon"
      "\t-h display this overview and exit\n"
      "\t-p create pidfile\n"
      "\t-q (ignored)\n"
      "\t-v (ignored)\n"
      "\t-V display version information and exit\n"
      "\n"
      "Use zero-argument version for normal operation.");
  puts(copyright);
  exit(EXIT_SUCCESS);
}

#endif	// NO_CONFIG


char *set_version(long long dummy)
{
  if (PACKAGE_VERSION) puts(PACKAGE_STRING);
  else puts("swapspace unknown");
  puts(copyright);
  exit(EXIT_SUCCESS);
}


#ifndef NO_CONFIG
/// Emit error message about configuration.  Returns false.
static bool config_error(const char msg[])
{
  fprintf(stderr, "%s: %s\n", configfile, msg);
  return false;
}

static bool value_error(const char keyword[], const char msg[])
{
  fprintf(stderr, "Configuration error: '%s': %s\n", keyword, msg);
  return false;
}


static bool handle_configitem(const char keyword[], const char *value)
{
  const struct option keyopt = { keyword, 0, 0, 0, 0 };
  const struct option *const opt =
    bsearch(&keyopt,
	options,
	sizeof(options)/sizeof(*options),
	sizeof(*options),
	optcmp);

  if (!opt) return value_error(keyword, "unknown configuration item");

  long long numarg = 0;
  size_t arglen = 0;
  if (value) arglen = strlen(value);

  const char *err = NULL;
  if (value) switch (opt->argtype)
  {
  case at_none:
    err = "does not take an argument";
    break;
  case at_num:
    if (!*value) err = "requires a numeric argument";
    else
    {
      char *endptr;
      numarg = strtoll(value, &endptr, 0);

      if (*endptr) switch (tolower(*endptr++))
      {
      case 'k': numarg *= KILO; break;
      case 'm': numarg *= MEGA; break;
      case 'g': numarg *= GIGA; break;
      case 't': numarg *= TERA; break;
      default: err = "invalid unit letter";
      }

      if (*endptr) err = "invalid numeric argument";
      else if (numarg < opt->min) switch (opt->min)
      {
      case 0:	err = "argument may not be negative";		break;
      case 1:	err = "argument must be greater than zero";	break;
      default:	err = "given value too small";			break;
      }
      else if (numarg > opt->max) err = "given value too large";
    }
    break;
  case at_str:
    if (arglen > opt->max) err = "string too long";
    break;
  }
  else if (opt->argtype == at_num || (opt->argtype == at_str && opt->min))
  {
    err = "requires an argument";
  }
  if (err) return value_error(keyword, err);

  char *const strdest = opt->setter(numarg);
  if (strdest && value) strcpy(strdest, value);

  return true;
}


static bool read_config(void)
{
  FILE *fp = fopen(configfile, "r");
  if (!fp)
  {
    const int err = errno;
    const bool nofile = (err == ENOENT);
    // TODO: It's also an error if nofile and -c option used
    if (!nofile) perror("Could not open configuration file");
    else if (!quiet) fputs("Using default configuration\n",stderr);
    return nofile;
  }

  while (fgets(localbuf, sizeof(localbuf), fp))
  {
    // Cut line short at hash character, if any
    int j;
    for (j=0; localbuf[j] && localbuf[j] != '#'; ++j);
    localbuf[j] = '\0';

    char key[100], val[PATH_MAX], dummy[2];
    // TODO: Check that quotes are closed properly
    if (sscanf(localbuf," %100[a-z_] = \"%"PMS"[^\"]\" %1s",key,val,dummy)==2 ||
	sscanf(localbuf," %100[a-z_] = %"PMS"s %1s",key,val,dummy) == 2)
    {
      if (!handle_configitem(key, val)) return false;
    }
    else if (sscanf(localbuf," %100[a-z_] %1s",key,dummy) == 1)
    {
      if (!handle_configitem(key, NULL)) return false;
    }
    else if (sscanf(localbuf," %1s",dummy) > 0)
    {
      return config_error("syntax error");
    }
  }

  if (ferror(fp))
  {
    perror("Error reading configuration file");
    return false;
  }

  return true;
}


/// Parse command line.  Clobbers localbuf.
static bool read_cmdline(int argc, char *argv[])
{
  assert(argv[argc-1]);
  assert(!argv[argc]);

  for (int i=1; i<argc; ++i)
  {
    assert(argv[i]);
    const char *nextarg = NULL;

    if (argv[i][0] != '-' || !argv[i][1])
    {
      fprintf(stderr, "Illegal argument: %s\n", argv[i]);
      return false;
    }
    const char o = argv[i][1];
    const char *optname;
    if (o == '-')
    {
      // Long option name (starting with double dash)
      optname = argv[i]+2;
      if (!optname[0] || !isalpha(optname[0]))
      {
	fprintf(stderr, "Illegal argument: %s\n", argv[i]);
	return false;
      }
      const char *const eqsign = strchr(optname, '=');
      if (eqsign)
      {
	// Option in '--foo=bar' style
	const size_t optnamelen = eqsign - optname;
	if (optnamelen >= sizeof(localbuf))
	{
	  fprintf(stderr, "Option name too long\n");
	  return false;
	}
	strncpy(localbuf, optname, optnamelen);
	localbuf[optnamelen] = '\0';
	optname = localbuf;
	nextarg = eqsign + 1;
      }
    }
    else
    {
      // Short option name (starting with single dash)
      int j;
      for (j=0;
	   j<sizeof(options)/sizeof(*options) && options[j].shortopt != o;
	   ++j);
      if (j == sizeof(options)/sizeof(*options))
      {
	fprintf(stderr, "Illegal option: -%c\n", o);
	return false;
      }
      optname = options[j].name;

      if (argv[i][2]) nextarg = argv[i] + 2;
    }

    if (!nextarg && argv[i+1] && (argv[i+1][0]!='-'||isdigit(argv[i+1][1])))
    {
      ++i;
      nextarg = argv[i];
    }
    if (!handle_configitem(optname, nextarg)) return false;
  }
  return true;
}
#endif	// NO_CONFIG


bool configure(int argc, char *argv[])
{
#ifndef NO_CONFIG

  assert(options_okay());

  /* I'm not too proud of this.  Read command line first, because it may change
   * where the configuration file is.  Then read the configuration file, and
   * read the command line again to override any options that may be set by
   * both.
   */
  if (!read_cmdline(argc, argv) || !read_config()) return false;
  read_cmdline(argc, argv);
  

#else

  // Unconfigurable build; we only do a minimal options loop.
  for (int a=1; argv[a]; ++a)
  {
    if (argv[a][0] != '-')
    {
      fprintf(stderr, "Invalid argument: '%s'.  Try the -h option.\n", argv[a]);
      return false;
    }
    switch (argv[a][1])
    {
    case 'd':			break;	// Quietly ignored
    case 'e': set_erase(0);	break;
    case 'h': short_usage();	break;	// Does not return
    case 'p': set_pidfile(0);	break;
    case 'q':			break;	// Quietly ignored
    case 'v':			break;	// Quietly ignored
    case 'V': set_version(0);	break;	// Does not return
    default:
      fprintf(stderr,"Invalid option: '%s'. Try -h instead.\n",argv[a]);
      return false;
    }
  }

#endif

  if (!main_check_config() ||
      !memory_check_config() ||
      !swaps_check_config() ||
      !swapfs_large_enough())
    return false;

  if (inspect) exit(EXIT_SUCCESS);

  return to_swapdir();
}


