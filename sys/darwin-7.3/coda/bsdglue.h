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


/* remove parse error induced by __FBSDID. Maybe we should do something sensible with this later on */


#ifdef DARWIN
#include <specdev.h>

#define __FBSDID(x)
#define v_vflag v_flag
#define VV_ROOT VROOT
#define VV_TEXT VTEXT

/* The following macros alter the usage of struct thread to struct proc */
#define THREAD struct proc
#define CURTHREAD current_proc();
#define THREAD2PROC td
#define uio_td uio_procp
#define cn_thread cn_proc
#define td_ucred p_ucred
#define a_td a_p


#define devsw(x) (&cdevsw[major(dev)])
#define VOP_OPEN(x,y,z,w,t) VOP_OPEN(x,y,z,w)
#define VOP_BMAP(x,y,z,w,t,r) VOP_BMAP(x,y,z,w,t)
#define NDFREE(x,y)
#define lockstatus(x,y) lockstatus(x)
#define VFS_VGET(x,y,z,w) VFS_VGET(x,y,w)
#define getnewvnode(x,y,z,w) getnewvnode(VT_OTHER,y,z,w);

#else

#define THREAD struct thread
#define CURTHREAD curthread
#define THREAD2PROC td->td_proc

#endif /* !DARWIN */
