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

#include <coda/coda_psdev.h>

int vrefcnt(struct vnode *vp)
{
    int i;
   
    i = vp->v_usecount;

    return (i);
}
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

#define VFSTOHFS(MP) ((struct hfsmount *)(MP)->mnt_data)

dev_t hfsdev( struct mount *mp)
{
    return VFSTOHFS(mp)->hfs_raw_dev;
}

extern int coda_debug_locks;

#ifdef DEBUG_VFS_LOCKS
void assert_vop_locked(struct vnode *vp, char *f, int l)
{
    if(coda_debug_locks && vp && (vp->v_type != VCHR) && (vp->v_type != VBAD) && (VOP_ISLOCKED(vp) == 0))
    {
        printf("Vnode at %p is not locked, though it should: %s(%d)\n",(void *)vp, f, l);
    }
}

void assert_vop_unlocked(struct vnode *vp, char *f, int l)
{
    if(coda_debug_locks && vp && (vp->v_type != VCHR) && (vp->v_type != VBAD) && VOP_ISLOCKED(vp))
    {
        printf("Vnode at %p is locked, though it should not: %s(%d)\n",(void *)vp, f, l);
    }
}
#endif

#include <mach/mach_types.h>
#include <miscfs/devfs/devfs.h>


struct slock coda_instances_lock;
int coda_instances=0;

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

//static struct vnodeopv_desc coda_vnodeop_opv_desc =
//{ &coda_vnodeop_p, coda_vnodeop_entries };

kern_return_t darwin_coda_start (kmod_info_t * ki, void * d);
kern_return_t darwin_coda_stop (kmod_info_t * ki, void * d);

kern_return_t darwin_coda_start (kmod_info_t * ki, void * d)
{
    struct vfsconf *coda_vfsconf;
    boolean_t old_funnel_status;
    struct vnodeopv_entry_desc *ved_p;
    
    int (**coda_vnodeop_entries)();    
    int (***coda_vnodeop_entries_p)();
    int i;
    int rv=0;
    
    
    old_funnel_status = thread_funnel_set(kernel_flock, TRUE);
    
    MALLOC(coda_vfsconf, void *, sizeof(struct vfsconf), M_TEMP, M_WAITOK);
    bzero(coda_vfsconf, sizeof(struct vfsconf));
    
    simple_lock_init(&coda_instances_lock);
    coda_vfsconf->vfc_vfsops = &coda_vfsops;
    strncpy(coda_vfsconf->vfc_name, "coda", 4);
    
    coda_vfsconf->vfc_typenum=maxvfsconf++; // ••• Bad to use a system global here!!
    coda_vfsconf->vfc_refcount	= 0;
    coda_vfsconf->vfc_flags		= 0;
    coda_vfsconf->vfc_mountroot	= NULL;			// Can't mount as root
    coda_vfsconf->vfc_next		= NULL;
    
    coda_vnodeop_entries_p = coda_vnodeop_opv_desc.opv_desc_vector_p;
    MALLOC(*coda_vnodeop_entries_p, int_fn_ptr_t *, vfs_opv_numops*sizeof(int_fn_ptr_t), M_TEMP, M_WAITOK);
    bzero(*coda_vnodeop_entries_p, vfs_opv_numops*sizeof(int_fn_ptr_t));
    coda_vnodeop_entries = *coda_vnodeop_entries_p;
    
    
    for(i=0; coda_vnodeop_opv_desc.opv_desc_ops[i].opve_op; i++ )
    {
        ved_p=&(coda_vnodeop_opv_desc.opv_desc_ops[i]);
        
        if(ved_p->opve_op->vdesc_offset == 0 && 
           ved_p->opve_op->vdesc_offset != VOFFSET ( vop_default ) )
        {
            printf("coda_kextload: operation %s not listed in vfs_op_descs.\n", ved_p->opve_op->vdesc_name);
            thread_funnel_set(kernel_flock, old_funnel_status);
            return KERN_FAILURE;
        }
        coda_vnodeop_entries[ved_p->opve_op->vdesc_offset]=ved_p->opve_impl;
    }
    
    coda_vnodeop_entries_p = coda_vnodeop_opv_desc.opv_desc_vector_p;
    coda_vnodeop_entries = *coda_vnodeop_entries_p;
    
    /* Check that we have a default routine */
    if(coda_vnodeop_entries[VOFFSET(vop_default)] == NULL)
    {
        printf("coda_kextload: A vnode entry has no default routine.");
        thread_funnel_set(kernel_flock, old_funnel_status);
        return KERN_FAILURE;
    }
    /* Fill in default routine in all unset entries */
    for(i=0; i<vfs_opv_numops; i++)
    {
        if(coda_vnodeop_entries[i] == NULL)
        {
            coda_vnodeop_entries[i]=coda_vnodeop_entries[VOFFSET(vop_default)];
	}
        //  printf("coda_vnodeop_entries[%d]=%X\n",i,coda_vnodeop_entries[i]);
    }
    
    // Ok, vnode vectors are set up, vfs vectors are set up, add it in
    rv=vfsconf_add(coda_vfsconf);
    if(rv)
    {
        printf("coda_kextload: vfsconf_add failed. Return value=%d\n",rv);
        thread_funnel_set(kernel_flock, old_funnel_status);
        return KERN_FAILURE;
    }
    
    if(coda_vfsconf)
    {
        FREE (coda_vfsconf, M_TEMP);
        coda_vfsconf=NULL;
    }
    
    rv=coda_cdevsw_init();
    
    if(rv < 0)
    {
        printf("coda_kextload: control device init failed\n");
        return KERN_FAILURE;
    }
    
    coda_ufsops=*ffs_vnodeop_opv_desc.opv_desc_vector_p;
    
    coda_debugon();
    
    printf("Coda kernel extension loaded\n"); 
    thread_funnel_set(kernel_flock, old_funnel_status);
    return KERN_SUCCESS;
}

kern_return_t darwin_coda_stop (kmod_info_t * ki, void * d) 
{
    boolean_t old_funnel_status;
    int rv=0;
    
    old_funnel_status = thread_funnel_set(kernel_flock, TRUE);
    printf(("coda_kextunload: Unloading coda kernel extension"));
    simple_lock_init(&coda_instances_lock);
    if(coda_instances > 0)
    {
        simple_unlock(&coda_instances_lock);
        thread_funnel_set(kernel_flock, old_funnel_status);
        return KERN_FAILURE;
    }
    simple_unlock(&coda_instances_lock);
    rv=vfsconf_del("coda");
    if (rv)
    {
        printf ("coda_kextload: vfsconf_delfailed. Return value=%d\n",rv);
        thread_funnel_set(kernel_flock, old_funnel_status);
        return KERN_FAILURE;
    }
    
    FREE(*coda_vnodeop_opv_desc.opv_desc_vector_p,M_TEMP);
    
    coda_cdevsw_uninit();
    
    printf(("coda_kextunload: coda kernel extension unloaded"));
    thread_funnel_set(kernel_flock, old_funnel_status);
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
            printf("coda_init: failed to allocate a major number!\n");
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
    printf("coda_cdevsw_uninit: cdevsw_remove(%d,%X) returned %d\n", coda_vc_major, &coda_cdevsw, rv);
    if(rv == coda_vc_major)
        coda_vc_major=-1;
}


