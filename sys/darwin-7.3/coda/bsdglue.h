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
int         vrefcnt(struct vnode *vp);
extern void delay();
extern int  memcmp(const void *, const void *, size_t);



#ifdef DEBUG_VFS_LOCKS

#define	ASSERT_VOP_LOCKED(vp, s)	assert_vop_locked((vp), __FILE__, __LINE__)
#define	ASSERT_VOP_UNLOCKED(vp, s)	assert_vop_unlocked((vp), __FILE__, __LINE__)

#else /* !DEBUG_VFS_LOCKS */

#define	ASSERT_VOP_LOCKED(vp, s)
#define	ASSERT_VOP_UNLOCKED(vp, s)

#endif /* DEBUG_VFS_LOCKS */


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
#define USE_VGET

#define devsw(x) (&cdevsw[major(dev)])
//#define VOP_OPEN(x,y,z,w,t) VOP_OPEN(x,y,z,w) - Changes are in the code instead. old redef kept for reference
//#define VOP_BMAP(x,y,z,w,t,r) VOP_BMAP(x,y,z,w,t)  - Changes are in the code instead. old redef kept for reference
#define NDFREE(x,y)
#define lockstatus(x,y) lockstatus(x)
#define lockdestroy(x)
// #define VFS_VGET(x,y,z,w) VFS_VGET(x,y,w) - Changes are in the code instead. old redef kept for reference
#define getnewvnode(x,y,z,w) getnewvnode(VT_OTHER,y,z,w);
#define selwakeuppri(p,x) selwakeup(p)
#define vfs_object_create(vp, p, cred) (0)
#define VFS_SET(x,y,z) void _no_coda_vfs_set(){}

#define udev2dev(x) (x)
//#define cdev_t dev_t
#define dev2udev(x) (x)
#define VNODEOP_SET(x)
#define vnode_pager_setsize(x,y) 
/* Defining NOTFB31 activates old insqe qnd remque definitions in sys/queue.h */
#define NOTFB31 (1)
/* PROC_LOCK seem to be unused in Darwin, there are no signs in the Darwin code that the proc block is
locked at a number of places almost identical with FreeBSD  I'm leaving the calls in the code
and define PROC_LOCK and PROC_UNLOCK to be null for the time being */
#define PROC_LOCK(x)
#define PROC_UNLOCK(x)
#define udev_t dev_t







#else

#define THREAD struct thread
#define CURTHREAD curthread
#define THREAD2PROC td->td_proc

#endif /* !DARWIN */
