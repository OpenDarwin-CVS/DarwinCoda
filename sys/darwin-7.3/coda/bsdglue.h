/*
 *  bsdglue.h
 *  darwin_coda
 *
 *  This file alters compilation of the original FreeBSD coda source to suit the 
 *  Darwin opersting system as much as possible. The object is to reduce the number
 *  of differences between the FreeBSD code and the Darwin code.
 *
 * It must be #included in every .c file.
 *
 *  Created by Christer Bernérus on Sat May 01 2004.
 *  Copyright (c) 2004 Christer Bernérus. All rights reserved.
 *  This code is released under the BSD License.
 *
 */


/* remove parse error induced bu __FBSDID. Maybe we should do something sensible with this later on */
#define __FBSDID(x)

#ifdef __APPLE__
#define DARWIN
#endif

#ifdef DARWIN
#include <specdev.h>

#define v_vflag v_flag
#define VV_ROOT VROOT
#define VV_TEXT VTEXT
#define THREAD struct proc
#define CURTHREAD current_proc();
#define THREAD2PROC td
#else
#define THREAD struct thread
#define CURTHREAD curthread
#define THREAD2PROC td->td_proc
#endif /* DARWIN */
