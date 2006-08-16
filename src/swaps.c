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
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
*/
#include "env.h"

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/swap.h>
#include <sys/types.h>

#include "log.h"
#include "opts.h"
#include "state.h"
#include "support.h"
#include "swaps.h"


// Try to use O_LARGEFILE
#ifndef O_LARGEFILE
#define O_LARGEFILE 0
#endif

// Don't follow symlinks, if possible; they may pose a security risk 
#ifndef O_NOFOLLOW
#define O_NOFOLLOW 0
#endif

static char swappath[PATH_MAX] = "/var/lib/swapspace";
static size_t swappath_len;


/// Smallest allowed swapfile size
static memsize_t min_swapsize = 4*MEGA;
/// Largest allowed swapfile size
/** Don't set this too low.  The program will learn if it runs into file size
 * limits.
 */
static memsize_t max_swapsize = 2*TERA;

/// Truncate n to a multiple of memory page size
static memsize_t trunc_to_page(memsize_t n)
{
  return n & ~((memsize_t)getpagesize()-1);
}

/// Round n upwards to multiple of page size
static memsize_t ext_to_page(memsize_t n)
{
  return trunc_to_page(n) + getpagesize()-1;
}


#ifndef NO_CONFIG
char *set_swappath(long long dummy)
{
  return swappath;
}
char *set_min_swapsize(long long size)
{
  min_swapsize = trunc_to_page(size);
  return NULL;
}
char *set_max_swapsize(long long size)
{
  max_swapsize = trunc_to_page(size);
  return NULL;
}

bool paranoid = false;
char *set_paranoid(long long dummy)
{
  paranoid = true;
  return NULL;
}
#endif


#ifndef NO_CONFIG
bool swaps_check_config(void)
{
  CHECK_CONFIG_ERR(min_swapsize > max_swapsize);
  CHECK_CONFIG_ERR(min_swapsize < 10*getpagesize());

  if (swappath[0] != '/')
  {
    logm(LOG_ERR,
	"Swap path is not absolute (must start with '/'): '%s'",
	swappath);
    return false;
  }

  return true;
}
#endif


/// Change to swap directory, canonicalizing swappath in the process
bool to_swapdir(void)
{
  if (chdir(swappath) == -1)
  {
    const int err = errno;
    bool please_reinstall = false;
    log_perr_str(LOG_ERR, "Could not cd to swap directory", swappath, errno);
    switch (err)
    {
    case ENOENT:	// Does not exist
    case ENOTDIR:	// Is not a directory
#ifdef ELOOP
    case ELOOP:		// Leads to a link loop (or too many link levels anyway)
#endif
      please_reinstall = true;
      break;

    case EACCES:
      // If we're running as root, we definitely should have this permission!
      please_reinstall = (geteuid() == 0);
      break;
    }
    if (please_reinstall)
      logm(LOG_ERR, "swapspace installed incorrectly.  Please reinstall!");
    return false;
  }

  swappath_len = strlen(swappath);

#ifndef NO_CONFIG
  // Get rid of any "/./", "//", and "/../" clutter that might be in swappath.
  // This is needed because we want to recognize our swapfiles in /proc/swaps,
  // which will list them with the canonicalized version of swappath.
  if (!getcwd(swappath, sizeof(swappath)))
  {
    logm(LOG_CRIT, "Swap path too long");
    return false;
  }

  // Remove trailing slash, if any
  if (swappath[swappath_len-1] == '/')
  {
    --swappath_len;
    swappath[swappath_len] = '\0';
  }
  for (int i=swappath_len-1; i >= 0; --i) if (isspace(swappath[i]))
  {
    logm(LOG_ERR, "Not supported: swap path contains whitespace");
    return false;
  }
#endif

  return true;
}


struct Swapfile
{
  memsize_t size;
  memsize_t used;
  long long created;
  /// Has this swapfile been spotted in /proc/swaps?
  bool observed_in_wild;
};

static int sequence_number = 0;

/// Power-of-two defining how many active swapfiles to support
/** Since swapspace allocates swapfiles of increasing sizes, there is probably
 * little need for a large number of swapfile slots.
 */
#define MAXSWAPS_SHIFT 5
#define MAX_SWAPFILES (1 << MAXSWAPS_SHIFT)

/// Array of swapfile descriptors
/** The last element remains zeroed for use as a sentry.
 */
static struct Swapfile swapfiles[MAX_SWAPFILES+1];

/// Have we been able to verify that /proc/swaps is in the expected format?
static bool proc_swaps_read_ok = false;



/// Print status information to stdout
void dump_stats(void)
{
  logm(LOG_INFO, "clock: %lld", (long long)runclock);

  dump_state();
  dump_memory();

  // Count active swapfiles.  Note that we don't remember this anywhere; it's
  // rarely needed (only when requested), it's not very costly to derive, and
  // keeping a redundant counter is just asking for minor bugs.
  int activeswaps = 0;
  for (int i=0; i<MAX_SWAPFILES; ++i) if (swapfiles[i].size) ++activeswaps;
  logm(LOG_INFO, "swapfiles in use: %d", activeswaps);
  if (activeswaps)
  {
    logm(LOG_INFO,
	"file            size            used         created  seen");
    for (int i=0; i<MAX_SWAPFILES; ++i) if (swapfiles[i].size)
      logm(LOG_INFO,
	  "%4d%16lld%16lld%16lld  %d",
	  i,
	  swapfiles[i].size,
	  swapfiles[i].used,
	  swapfiles[i].created,
	  (int)swapfiles[i].observed_in_wild);
  }
}



/// Return next swapfile number after i
static inline int inc_swapno(int i)
{
  return (i+1) % MAX_SWAPFILES;
}


/// Get filesystem info for swapdir, but retry once if interrupted by signal
static bool statvfs_wrapper(struct statvfs *i)
{
  int fail = statvfs(swappath,i);
  if (fail == -1 && errno == EINTR) fail = statvfs(swappath,i);
  if (fail == -1)
    log_perr_str(LOG_ERR,
	"Could not get filesystem information for swap directory",
	swappath,
	errno);

  return !fail;
}


/// Convert filesystem figure given in blocks to bytes
/** We do this to get rid of ugly casts that we might forget.  The real work is
 * not in the function body but in the prototype: now the promotion to memsize_t
 * is done implicitly in the call itself!
 */
static memsize_t fs_size(memsize_t n, memsize_t blocksize)
{
  return n * blocksize;
}


memsize_t swapfs_free(void)
{
  struct statvfs fsinfo;

  if (unlikely(!statvfs_wrapper(&fsinfo))) return 0;

  // Return free space available to non-root users rather than the space that is
  // really free, so we leave some margin for the superuser to work in if the
  // disk fills up.
  return fs_size(fsinfo.f_bavail, fsinfo.f_bsize);
}


memsize_t swapfs_size(void)
{
  struct statvfs fsinfo;

  if (unlikely(!statvfs_wrapper(&fsinfo))) return 0;

  return fs_size(fsinfo.f_blocks, fsinfo.f_bsize);
}


/// Turn an existing file into an active swap.  Clobbers localbuf.
/**
 * @return Whether file was activated successfully.
 */
static bool enable_swapfile(const char file[])
{
  runcommand("mkswap", file);
  const bool ok = (swapon(file, 0) == 0);
  if (unlikely(!ok))
    log_perr_str(LOG_ERR, "Could not enable swapfile", file, errno);
  return ok;
}



/// Write arbitrary data to swapfile.  Clobbers localbuf.
/** Fill a swapfile with the specified number of bytes of data.  We don't care
 * what the data is.
 *
 * If an error occurs, this function will write fewer bytes than specified.  It
 * may write more than specified otherwise, since file size will be rounded up
 * to a multiple of page size.
 *
 * Errors are not logged here; this function mostly works as a write() that
 * ignores EINTR (unless the signal was an exit request).  Otherwise, refer to
 * errno to diagnose error conditions.
 *
 * @param fd file descriptor to write to
 * @param bytes number of bytes to write
 *
 * @return number of bytes written
 */
static memsize_t write_data(int fd, memsize_t bytes, bool persevere)
{
  // Round upwards to multiple of page size
  bytes = ext_to_page(bytes);

  /* Zero buffer before using it to write data to swapfile.  This doesn't do
   * much for security (if an attacker can get to your swapfiles you don't have
   * a lot of that anyway) but perhaps some filesystems may recognize all-zero
   * pages and compress them, or reduce cache usage, or something.
   */
  memset(localbuf, 0, sizeof(localbuf));

  memsize_t written = 0;
  ssize_t block;

  // Main writing loop.  Repeat until we've written all bytes, or until we fail.
  // EINTR is not considered a failure if we've been told to persevere.
  do
  {
    block = write(fd, localbuf, MIN(sizeof(localbuf),bytes-written));
    written += block;
  }
  while ((written < bytes) && (block >= 0 || (errno == EINTR && persevere)));

  // Byte count may end up a little low if write() returns -1.  That's okay.

  return written;
}


/// Populate swapfile by writing data to it
/**
 * @return Real size of created file, or zero on failure
 */
static memsize_t fill_swapfile(const char file[], int fd, memsize_t size)
{
  //
  memsize_t bytes = write_data(fd, size, false);

  if (unlikely(bytes < size))
  {
    const int err = errno;
    log_perr_str(LOG_ERR, "Error writing swapfile", file, errno);
    switch (err)
    {
    case EFBIG:
      // File too big.  Don't try creating files this large again.
      if (likely(bytes > 0 && max_swapsize > bytes))
      {
        max_swapsize = trunc_to_page(bytes);
#ifndef NO_CONFIG
        if (verbose)
	  logm(LOG_INFO, "Restricting swapfile size to %lld", (long long)bytes);
#endif
      }
      break;
    case ENOSPC:
    case EIO:
      // Disk full, or low-level I/O error.  Go into "diet" state.
      request_diet();
      break;
    default:
	logm(LOG_WARNING, "Unexpected error writing swap file");
    }
    bytes = 0;
  }

  return bytes;
}


/// Create file to be used as swap.  Clobbers localbuf.
/**
 * @param filename File to be created
 * @param size Desired size in bytes (but already rounded to page size)
 * @return Size of new swapfile, which may differ from requested size.  Zero 
 * indicates failure, in which case the file is deleted.
 */
static memsize_t make_swapfile(const char file[], memsize_t size)
{
  assert(min_swapsize <= max_swapsize);
  assert(max_swapsize == trunc_to_page(max_swapsize));
  assert(min_swapsize == trunc_to_page(min_swapsize));
  assert(size == trunc_to_page(size));

  if (unlikely(size < min_swapsize)) return 0;

  size = MIN(size, max_swapsize);

  unlink(file);

  const int fd=open(file, O_WRONLY|O_CREAT|O_EXCL|O_LARGEFILE, S_IRUSR|S_IWUSR);
  if (unlikely(fd == -1))
  {
    log_perr_str(LOG_ERR, "Could not create swapfile", file, errno);
    return 0;
  }

  if (unlikely(!fill_swapfile(file, fd, size)))
  {
    size = 0;
    unlink(file);
  }
  close(fd);
  
  return size;
}


/// Information about a swapfile
struct swapfile_info
{
  char name[PATH_MAX];
  int seqno;
  memsize_t size;
  memsize_t used;
};


static bool valid_swapfile(const char filename[], int *seqno)
{
  if (unlikely(!*filename)) return false;

  char *endptr;
  long seql = strtol(filename, &endptr, 10);
  if (unlikely(*endptr || seql < 0 || seql >= MAX_SWAPFILES)) return false;
  *seqno = (int)seql;
  return true;
}


static memsize_t filesize(const char name[])
{
  int fd = open(name, O_RDONLY|O_LARGEFILE|O_NOFOLLOW);
  if (unlikely(fd == -1))
  {
    log_perr_str(LOG_WARNING, "Can't determine size of", name, errno);
    return -1;
  }

  memsize_t pos = SEEK(fd, 0, SEEK_END);
  if (unlikely(pos == -1))
    log_perr_str(LOG_WARNING, "Can't determine size of", name, errno);

  return pos;
}


bool activate_old_swaps(void)
{
  DIR *dir = opendir(".");
  if (unlikely(!dir))
  {
    log_perr(LOG_ERR, "Cannot read swap directory", errno);
    return false;
  }
  for (struct dirent *d = readdir(dir); d; d = readdir(dir))
  {
    int seqno;
    if (valid_swapfile(d->d_name, &seqno) && !swapfiles[seqno].size)
    {
#ifndef NO_CONFIG
      if (!quiet) logm(LOG_INFO, "Found old swapfile '%d'", seqno);
#endif
      const memsize_t size = filesize(d->d_name); 
      if (likely(size > min_swapsize && enable_swapfile(d->d_name)))
      {
	swapfiles[seqno].size = size;
      }
      else
      {
#ifndef NO_CONFIG
	if (!quiet) logm(LOG_NOTICE, "Deleting unusable swapfile '%d'", seqno);
#endif
	unlink(d->d_name);
      }
    }
  }
  if (unlikely(closedir(dir)==-1)) perror("Error closing swap directory");

  if (!proc_swaps_parsed() && unlikely(!read_proc_swaps())) return false;

  return true;
}


static FILE *open_proc_swaps(void)
{
  FILE *fp = fopen("/proc/swaps", "r");
  if (unlikely(!fp)) log_perr(LOG_ERR, "Could not open /proc/swaps", errno);
  return fp;
}


/// Check that /proc/swaps header line, as found in buf, matches expected format
static bool check_proc_swaps_header(const char buf[])
{
  char c;
  if (unlikely(sscanf(buf, "Filename Type Size Used %c",&c) < 1))
  {
    logm(LOG_ERR, "/proc/swaps is not in the expected format");
    return false;
  }
  proc_swaps_read_ok = true;
  return true;
}


/// Read next swapfile description from /proc/swaps.  Clobbers localbuf.
/** Use to iterate the swapspace-managed swapfiles in /proc/swaps.  Partitions,
 * as well as non-swapspace swapfiles, are ignored.  Status of known swapfiles
 * found in the list is updated along the way.
 *
 * Returns false if no more swapfile descriptions were found; in that case,
 * result will contain garbage.  Check ferror(fp) to distinguish eof from error.
 */
static bool get_swapfile_status(FILE *fp, struct swapfile_info *result)
{
  while (fgets(localbuf, sizeof(localbuf), fp))
  {
    char type[100];
    char c;
    const int x=sscanf(localbuf,
	"%"PMS"s %100s %lld %lld %c",
	result->name,
	type,
	&result->size,
	&result->used,
	&c);
    if (unlikely(x < 5) && likely(localbuf[0]))
    {
      /* Nasty special case: if /proc/swaps is nonempty, it should have a header
       * line of a fixed format that we'd like to check.  Unfortunately, some
       * kernels including at least 2.6.10 (and who knows how many else) have a
       * bug that means the line may disappear if the oldest swap is disabled
       * while newer ones remain active.
       *
       * Also, looking at the kernel code, it seems unlikely that this can be
       * fixed cleanly without a vfs interface change.  Since we're working
       * around that bug here without giving up on our chance to verify the
       * file's format entirely, we may as well take the possibility into
       * account that an attempted kernel fix could occasionally put the header
       * line in the wrong place--i.e., not at the top of /proc/swaps but
       * somewhere further down.
       */
      if (!check_proc_swaps_header(localbuf))
      {
        logm(LOG_ERR, "Parse error in /proc/swaps: '%s'", localbuf);
        return false;
      }
    }

    // /proc/swaps appears to report sizes in 1 KB blocks
    result->size *= KILO;
    result->used *= KILO;

    if (likely(strcmp(type, "file") == 0) &&
	likely(strncmp(result->name, swappath, swappath_len) == 0) &&
	likely(result->name[swappath_len] == '/') &&
	likely(valid_swapfile(result->name+swappath_len+1, &result->seqno)))
    {
      // Found what looks to be one of our swapfiles.  Update our list.
      if (unlikely(!swapfiles[result->seqno].size))
      {
	// We didn't know about this swapfile yet.  Adopt it.
#ifndef NO_CONFIG
	if (!quiet) logm(LOG_NOTICE, "Detected swapfile '%d'", result->seqno);
#endif
	swapfiles[result->seqno].created = runclock;
      }
#ifndef NO_CONFIG
      else if (unlikely(swapfiles[result->seqno].size != result->size))
      {
	// Size wasn't exactly what we expected.  This is not unusual; there are
	// likely to be a few pages of overhead.
	const memsize_t expected = swapfiles[result->seqno].size,
	      		found = result->size;

	if (unlikely(swapfiles[result->seqno].observed_in_wild))
	  log_discrep(LOG_NOTICE,"size changed",result->seqno,expected,found);
	else if (unlikely(found > expected))
	  log_discrep(LOG_INFO,
	      "larger than expected",
	      result->seqno,
	      expected,
	      found);
	else if (!quiet && unlikely(found+2*getpagesize() < expected))
	  log_discrep(LOG_INFO,
	      "smaller than expected",
	      result->seqno,
	      expected,
	      found);
      }
      if (!quiet && unlikely(result->used > result->size))
	log_discrep(LOG_NOTICE,
	    "usage exceeds size",
	    result->seqno,
	    result->used,
	    result->size);
#endif

      swapfiles[result->seqno].size = result->size;
      swapfiles[result->seqno].used = result->used;
      swapfiles[result->seqno].observed_in_wild = true;
      return true;
    }
  }
  return false;
}


bool proc_swaps_parsed(void)
{
  return proc_swaps_read_ok;
}


/// Read /proc/swaps and set up swapfiles[] accordingly.  Clobbers localbuf.
bool read_proc_swaps(void)
{
  FILE *const fp = open_proc_swaps();
  if (unlikely(!fp)) return false;

  for (int i=0; i<MAX_SWAPFILES; ++i) swapfiles[i].observed_in_wild = false;

  struct swapfile_info inf;
  while (get_swapfile_status(fp, &inf));
  const bool result = !ferror(fp);
  fclose(fp);

  // Scratch from swapfiles[] any swapfiles that were deactivated from outside
  for (int i=0; i<MAX_SWAPFILES; ++i)
    if (unlikely(swapfiles[i].size && !swapfiles[i].observed_in_wild))
      swapfiles[i].size = 0;

  return result;
}


/// Disable swapfile and delete it.  Clobbers localbuf.
static bool retire_swapfile(int file)
{
  assert(file >= 0);
  assert(file < MAX_SWAPFILES);

  /* If swapoff() fails, we may be in an awkward situation.  If we don't delete
   * the swapfile, we may be leaving an unused swapfile behind on the disk.
   * That would not be very nice of us.  If the swapfile is still in use, on the
   * other hand, then we run into another problem.  Since swapoff() works by
   * filename, if we are able to delete the file then we won't be able to
   * deactivate it without disabling swapping altogether.
   *
   * Finally, we also need to avoid reusing names of deleted swapfiles while
   * they are still in use or things would get horribly confused.
   */
  char namebuf[30];
  snprintf(namebuf, sizeof(namebuf), "%d", file);
#ifndef NO_CONFIG
  if (!quiet) logm(LOG_NOTICE, "Retiring swapfile '%d'", file);
#endif
  if (unlikely(swapoff(namebuf) == -1)) return false;

#ifndef NO_CONFIG
  int fd = -1;
  if (paranoid)
    fd = open(namebuf, O_WRONLY|O_LARGEFILE|O_NOFOLLOW);
#endif

  unlink(namebuf);

#ifndef NO_CONFIG
  if (fd != -1) write_data(fd, swapfiles[file].size, true);
#endif

  swapfiles[file].size = 0;
  return true;
}


/// Is given swapfile eligible for retirement?
static bool retirable(int file, memsize_t maxsize)
{
  assert(file >= 0);
  assert(file < MAX_SWAPFILES);
  // TODO: Include usage in calculations?  Like "free the most unused space"?
  return swapfiles[file].size && swapfiles[file].size <= maxsize;
}


/// Find a file to retire, or return MAX_SWAPFILES if none available
/** Policy is to offer the largest swapfile that is not bigger than target.
 */
static int find_retirable(memsize_t target)
{
  if (!read_proc_swaps()) return MAX_SWAPFILES;
  int best=MAX_SWAPFILES;
  assert(!swapfiles[best].size);
  for (int i=0; i<MAX_SWAPFILES; ++i)
    if (retirable(i, target) && swapfiles[i].size >= swapfiles[best].size)
      best = i;
  return best;
}


bool retire_all(void)
{
  bool ok = true;

  for (int i=0; i<MAX_SWAPFILES; ++i)
    if (swapfiles[i].size && !retire_swapfile(i)) ok = false;

  return ok;
}


/// Find a free swapfile slot, or return last if none available
static int find_free(int last)
{
  assert(last >= 0);
  assert(last < MAX_SWAPFILES);
  int i;
  for (i = last+1; i < MAX_SWAPFILES && swapfiles[i].size; ++i);
  if (i >= MAX_SWAPFILES) for (i = 0; i < last && swapfiles[i].size; ++i);
  return i;
}


bool alloc_swapfile(memsize_t size)
{
  /* Round request to page size, then add a bit for swapfile overhead.  Clever
   * readers will notice that this relies on getpagesize() returning a power of
   * two.
   */
  size = trunc_to_page(size) + 2*getpagesize();
  const int newswap = find_free(sequence_number);
  if (unlikely(swapfiles[newswap].size)) return false;	// No free slot, sorry!

  if (unlikely(size > swapfs_free())) return false;	// Not enough disk space

  // We can allocate another swapfile.  Great.
#ifndef NO_CONFIG
  if (!quiet) logm(LOG_NOTICE, "Allocating swapfile '%d'", newswap);
#endif
  char file[30];
  snprintf(file, sizeof(file), "%d", newswap);
  swapfiles[newswap].size = make_swapfile(file, size);
  if (unlikely(!swapfiles[newswap].size)) return false;

  if (unlikely(!enable_swapfile(file)))
  {
    unlink(file);
    request_diet();
    return false;
  }

  sequence_number = inc_swapno(sequence_number);

  return true;
}


void free_swapfile(memsize_t maxsize)
{
  const int victim = find_retirable(maxsize);
  if (victim < MAX_SWAPFILES) retire_swapfile(victim);
}
