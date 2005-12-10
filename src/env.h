/*
This file is part of Swapspace.

Copyright (C) 2005, Software Industry Industry Promotion Agency (SIPA), Thailand
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
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

// Include this file from every source file, and before any system includes!
#ifndef SWAPSPACE_ENV_H
#define SWAPSPACE_ENV_H

#ifdef SUPPORT_LARGE_FILES
# define _LARGEFILE64_SOURCE
# define SEEK(fd,pos,whence) lseek64(fd,pos,whence)
#else
# define SEEK(fd,pos,whence) lseek(fd,pos,whence)
#endif

#endif

