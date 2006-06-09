/*
 * Copyright (C) Christer Bernerus, 2004. All rights reserved
 */
/*
 * Copyright (c) 2000-2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#include <coda/coda.h>
#include <coda/cnode.h>
#include <coda/coda_psdev.h>
#include <kern/thread.h>


/* 
 * Using VFSTOHFS directly within the coda routines opens up a significantly large can of worms in form
 * of a name clash between struct cnode (in HFS) and struct cnode (in CODA) Bringing in both coda.h and hfs.h
 * in the same source file also created zillions of other name clashes, so don't try to optimize away this
 * little function. You have been warned!
 */
/*
 * Warning: The struct definition below is stolen from hfs/hfs.h 
 * It is NOT complete and should only be used to make VFSTOHFS work
 *
 */
struct hfsmount {
    u_int32_t               hfs_flags;      /* see below */
    /* Physical Description */
    u_long                          hfs_phys_block_count;   /* Num of PHYSICAL blocks of volume */
    u_long                          hfs_phys_block_size;    /* Always a multiple of 512 */
    
    /* Access to VFS and devices */
    struct mount            *hfs_mp;                                /* filesystem vfs structure */
    struct vnode            *hfs_devvp;                             /* block device mounted vnode */
    dev_t                           hfs_raw_dev;                    /* device mounted */
};

#define VFSTOHFS(MP) ((struct hfsmount *)vfs_fsprivate(MP))

dev_t hfsdev( struct mount *mp)
{
    return VFSTOHFS(mp)->hfs_raw_dev;
}

extern int coda_debug_locks;
//extern funnel_t *kernel_flock;

#ifdef DEBUG_VFS_LOCKS
void assert_vop_locked(struct vnode *vp, char *f, int l)
{
    if(coda_debug_locks && vp && (vp->v_type != VCHR) && (vp->v_type != VBAD) && (VOP_ISLOCKED(vp) == 0))
    {
        myprintf(("Vnode at %p is not locked, though it should: %s(%d)\n",(void *)vp, f, l));
    }
}

void assert_vop_unlocked(struct vnode *vp, char *f, int l)
{
    if(coda_debug_locks && vp && (vp->v_type != VCHR) && (vp->v_type != VBAD) && VOP_ISLOCKED(vp))
    {
        myprintf(("Vnode at %p is locked, though it should not: %s(%d)\n",(void *)vp, f, l));
    }
}
#endif

#include <mach/mach_types.h>
#include <miscfs/devfs/devfs.h>

extern int (**coda_vnodeop_p)(void *);
extern struct vnodeopv_entry_desc coda_vnodeop_entries[];

static struct vnodeopv_desc coda_vnodeop_opv_desc =
{ &coda_vnodeop_p, coda_vnodeop_entries };

static struct vnodeopv_desc *coda_vnodeop_opv_desc_list[1] =
{
    &coda_vnodeop_opv_desc
};


extern struct vfsops coda_vfsops;
extern int vfs_opv_numops;
extern struct vnodeopv_desc coda_vnodeop_opv_desc;
extern struct vnodeopv_desc ffs_vnodeop_opv_desc;
extern int coda_cdevsw_init();
extern void coda_cdevsw_uninit();
extern void coda_debugon();
extern void coda_debugoff();

typedef int (*int_fn_ptr_t)();
int (**coda_ufsops)(void *);


kern_return_t darwin_coda_start (kmod_info_t * ki, void * d);
kern_return_t darwin_coda_stop (kmod_info_t * ki, void * d);

int coda_typenum;


static vfstable_t  coda_vfsconf;
extern int maxvfsconf;


kern_return_t darwin_coda_start (kmod_info_t * ki, void *d)
{
    struct vfs_fsentry vfe;

 
    

    int rv=0;
           
   // old_funnel_status = thread_funnel_set(kernel_flock, TRUE);
    
    
    vfe.vfe_vfsops = &coda_vfsops;
    vfe.vfe_vopcnt = 1;
    vfe.vfe_opvdescs = coda_vnodeop_opv_desc_list;
    strncpy(vfe.vfe_fsname, "coda" , MFSNAMELEN);
    vfe.vfe_flags = 0;
    vfe.vfe_fstypenum = coda_typenum = maxvfsconf++;
    
    vfe.vfe_reserv[0] = 0;
    vfe.vfe_reserv[1] = 0;
    
    rv = vfs_fsadd(&vfe, &coda_vfsconf);
    if(rv < 0)
    {
        myprintf(("coda_kextload: file system init failed\n"));
        return KERN_FAILURE;
    }
       
    rv=coda_cdevsw_init();
    
    if(rv < 0)
    {
        myprintf(("coda_kextload: control device init failed\n"));
        return KERN_FAILURE;
    }
    
    coda_ufsops=*ffs_vnodeop_opv_desc.opv_desc_vector_p;
    
    coda_debugon();
    
    myprintf(("Coda kernel extension loaded\n")); 
    
   // (void) thread_funnel_set(kernel_flock, old_funnel_status);
    return (KERN_SUCCESS);
}

kern_return_t darwin_coda_stop (kmod_info_t * ki, void * d) 
{
   // boolean_t old_funnel_status;
    int rv=0;
    
    
 //   old_funnel_status = thread_funnel_set(kernel_flock, TRUE);
    myprintf(("coda_kextunload: Unloading coda kernel extension"));
    
    rv = vfs_fsremove(coda_vfsconf);    if (rv)
    if(rv)
    {
        myprintf(("coda_kextload: vfsconf_fsremove failed. Return value=%d\n",rv));
      //  thread_funnel_set(kernel_flock, old_funnel_status);
        return KERN_FAILURE;
    }
    
    FREE(*coda_vnodeop_opv_desc.opv_desc_vector_p,M_TEMP);
    
    coda_cdevsw_uninit();
    
    myprintf(("coda_kextunload: coda kernel extension unloaded"));
 //   thread_funnel_set(kernel_flock, old_funnel_status);
    return KERN_SUCCESS;
}

static struct cdevsw coda_cdevsw = {
    .d_open =	vc_nb_open,
    .d_close =	vc_nb_close,
    .d_read =	vc_nb_read,
    .d_write =	vc_nb_write,
    .d_ioctl =	vc_nb_ioctl,
    .d_select =	vc_nb_select,
};

int coda_vc_major=-1;

int
coda_cdevsw_init()
{
    int     maj;
        
    if (coda_vc_major < 0) 
    {
        maj = cdevsw_add(-1, &coda_cdevsw);
        if (maj == -1) 
        {
            myprintf(("coda_cdevsw_init: failed to allocate a major number!\n"));
            return -1;
        }
        devfs_make_node(makedev(maj, 0), DEVFS_CHAR, UID_ROOT, GID_WHEEL, 0600, "cfs0", 0);
        coda_vc_major=maj;
    }
    return maj;
}


void
coda_cdevsw_uninit()
{
    int rv;
    if(coda_vc_major < 0)
        return;
    rv=cdevsw_remove(coda_vc_major, &coda_cdevsw);
    myprintf(("coda_cdevsw_uninit: cdevsw_remove(%d,%X) returned %d\n", coda_vc_major, &coda_cdevsw, rv));
    if(rv == coda_vc_major)
        coda_vc_major=-1;
}


