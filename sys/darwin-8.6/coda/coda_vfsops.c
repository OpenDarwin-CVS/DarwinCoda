/*
 *             Coda: an Experimental Distributed File System
 *                              Release 3.1
 * 
 *           Copyright (c) 1987-1998 Carnegie Mellon University
 *                          All Rights Reserved
 * 
 * Permission  to  use, copy, modify and distribute this software and its
 * documentation is hereby granted,  provided  that  both  the  copyright
 * notice  and  this  permission  notice  appear  in  all  copies  of the
 * software, derivative works or  modified  versions,  and  any  portions
 * thereof, and that both notices appear in supporting documentation, and
 * that credit is given to Carnegie Mellon University  in  all  documents
 * and publicity pertaining to direct or indirect use of this code or its
 * derivatives.
 * 
 * CODA IS AN EXPERIMENTAL SOFTWARE SYSTEM AND IS  KNOWN  TO  HAVE  BUGS,
 * SOME  OF  WHICH MAY HAVE SERIOUS CONSEQUENCES.  CARNEGIE MELLON ALLOWS
 * FREE USE OF THIS SOFTWARE IN ITS "AS IS" CONDITION.   CARNEGIE  MELLON
 * DISCLAIMS  ANY  LIABILITY  OF  ANY  KIND  FOR  ANY  DAMAGES WHATSOEVER
 * RESULTING DIRECTLY OR INDIRECTLY FROM THE USE OF THIS SOFTWARE  OR  OF
 * ANY DERIVATIVE WORK.
 * 
 * Carnegie  Mellon  encourages  users  of  this  software  to return any
 * improvements or extensions that  they  make,  and  to  grant  Carnegie
 * Mellon the rights to redistribute these changes without encumbrance.
 * 
 *  	@(#) src/sys/cfs/coda_vfsops.c,v 1.1.1.1 1998/08/29 21:14:52 rvb Exp $
 */
/* 
 * Mach Operating System
 * Copyright (c) 1989 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */

/*
 * This code was written for the Coda filesystem at Carnegie Mellon
 * University.  Contributers include David Steere, James Kistler, and
 * M. Satyanarayanan.  
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/coda/coda_vfsops.c,v 1.47 2003/09/13 01:13:56 tjr Exp $");

#include "vcoda.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/proc.h>

#include <coda/coda.h>
#include <coda/cnode.h>
#include <coda/coda_vfsops.h>
#include <coda/coda_venus.h>
#include <coda/coda_subr.h>
#include <coda/coda_opstats.h>

#include <uglyhack.h>

MALLOC_DEFINE(M_CODA, "CODA storage", "Various Coda Structures");

/* long long codadebug = CODADBGMSK(CODA_LOOKUP) | CODADBGMSK(CODA_SETATTR); */
long long codadebug = 0;
int coda_vfsop_print_entry = 0;
#define ENTRY    if(coda_vfsop_print_entry) PRINTENTRY
#define LEAVE    if(coda_vfsop_print_entry) PRINTLEAVE

struct vnode *coda_ctlvp;
struct coda_mntinfo coda_mnttbl[NVCODA]; /* indexed by minor device number */
int coda_islockedbyme(struct vnode *vp, int locktype);


/* structure to keep statistics of internally generated/satisfied calls */

struct coda_op_stats coda_vfsopstats[CODA_VFSOPS_SIZE];

#define MARK_ENTRY(op) (coda_vfsopstats[op].entries++)
#define MARK_INT_SAT(op) (coda_vfsopstats[op].sat_intrn++)
#define MARK_INT_FAIL(op) (coda_vfsopstats[op].unsat_intrn++)
#define MARK_INT_GEN(op) (coda_vfsopstats[op].gen_intrn++)

extern int coda_nc_initialized;     /* Set if cache has been initialized */
extern int vc_nb_open(dev_t, int, int, THREAD *);
//extern int coda_instances;
#ifdef DARWIN
extern dev_t hfsdev(struct mount *mp);
#endif /* DARWIN */

int
coda_vfsopstats_init(void)
{
	register int i;
	
	for (i=0;i<CODA_VFSOPS_SIZE;i++) {
		coda_vfsopstats[i].opcode = i;
		coda_vfsopstats[i].entries = 0;
		coda_vfsopstats[i].sat_intrn = 0;
		coda_vfsopstats[i].unsat_intrn = 0;
		coda_vfsopstats[i].gen_intrn = 0;
	}
	
	return 0;
}

static int coda_mount_type;
lck_grp_t * cnode_lck_grp;
lck_grp_attr_t *cnode_lck_grp_attr;



int
coda_init(struct vfsconf * vfsp)
{
    int error=0;
    ENTRY;
    coda_mount_type = vfsp->vfc_typenum; 
    cnode_lck_grp_attr = lck_grp_attr_alloc_init();
    lck_grp_attr_setstat(cnode_lck_grp_attr);
    cnode_lck_grp=lck_grp_alloc_init("coda_cnodes",cnode_lck_grp_attr);
    LEAVE;
    return error;
}

/*
 * cfs mount vfsop
 * Set up mount info record and attach it to vfs struct.
 */
/*ARGSUSED*/
int
coda_mount(struct mount *mp, vnode_t dvp, user_addr_t data, vfs_context_t context)
 {
    //struct volfs_mntdata *priv_mnt_data;
    struct vnode *root_vp;
    //struct volfs_vndata     *priv_vn_data;
    int error;
    //struct vnode_fsparam *vfsp;

    struct cnode *cp;
    char root_path[MNAMELEN];
    int len;
    dev_t dev;
    struct coda_mntinfo *mi;
    struct vfsstatfs *sfsp;
    CodaFid rootfid = INVAL_FID;
    CodaFid ctlfid = CTL_FID;

    ENTRY;

    coda_vfsopstats_init();
    coda_vnodeopstats_init();
    
    MARK_ENTRY(CODA_MOUNT_STATS);
    if (CODA_MOUNTED(mp)) {
	MARK_INT_FAIL(CODA_MOUNT_STATS);
        LEAVE;
	return(EBUSY);
    }

    /* Get mount point name */

    memset(root_path + len, '\0', MNAMELEN - len);

    dev = vnode_specrdev(dvp);
    //vnode_rele(dvp); 
    //NDFREE(ndp, NDF_ONLY_PNBUF);

    /*
     * See if the device table matches our expectations.
     */
    if (devsw(dev)->d_open != vc_nb_open)
    {
	MARK_INT_FAIL(CODA_MOUNT_STATS);
        LEAVE;
	return(ENXIO);
    }
    
    if (minor(dev) >= NVCODA || minor(dev) < 0) {
	MARK_INT_FAIL(CODA_MOUNT_STATS);
        LEAVE;
	return(ENXIO);
    }
    
    /*
     * Initialize the mount record and link it to the vfs struct
     */
    mi = &coda_mnttbl[minor(dev)];
    
    if (!VC_OPEN(&mi->mi_vcomm)) {
	MARK_INT_FAIL(CODA_MOUNT_STATS);
        LEAVE;
	return(ENODEV);
    }
    
    /* No initialization (here) of mi_vcomm! */
   // vfsp->mnt_data = (qaddr_t)mi;
    vfs_setfsprivate(mp, (qaddr_t)mi);
    vfs_getnewfsid (mp);

    mi->mi_vfsp = mp;
    mi->mi_started = 0;			/* XXX See coda_root() */
    
    rootfid.opaque[0] = 0;
    rootfid.opaque[1] = 0;
    rootfid.opaque[2] = 0;
    rootfid.opaque[3] = 0; 
    
    /*
     * Make a root vnode to placate the Vnode interface, but don't
     * actually make the CODA_ROOT call to venus until the first call
     * to coda_root in case a server is down while venus is starting.
     */
    cp = make_coda_node(&rootfid, mp, VDIR);
    if(cp==0) 
    {
        LEAVE;
        return ENOMEM;
    }
    SET_CNODE_NAME(root_path);
    root_vp = CTOV(cp);
    error = vn_getpath(root_vp, root_path, &len);
    if (error)
        return(error); 
	
/*  cp = make_coda_node(&ctlfid, vfsp, VCHR);
    The above code seems to cause a loop in the cnode links.
    I don't totally understand when it happens, it is caught
    when closing down the system.
 */
    cp = make_coda_node(&ctlfid, 0, VCHR);
    if(cp==0) 
    {
        LEAVE;
        return ENOMEM;
    }

    SET_CNODE_NAME("/dev/cfs0");

    coda_ctlvp = CTOV(cp);

    /* Add vfs and root_vp to chain of vfs hanging off mntinfo */
    mi->mi_vfsp = mp;
    mi->mi_rootvp = root_vp;
    
    /* set filesystem block size */
    struct vfsioattr ioattr;
    vfs_ioattr(mp,&ioattr);
    
    sfsp = vfs_statfs(mp);
    sfsp->f_bsize = 8192;
    /* Set f_iosize.  XXX -- inamura@isl.ntt.co.jp. 
        For vnode_pager_haspage() references. The value should be obtained 
        from underlying UFS. */
    /* Checked UFS. iosize is set as 8192 */
    sfsp->f_iosize = 8192;
    //sbp->f_fsid.val[0] = (long) dev;
    //sbp->f_fsid.val[1] = vfs_typenum(mp);
    
    /* f_fstypename, f_mntonname, f_mntfromname are set by VFS */
    
    // memcpy(vfsp->mnt_stat.f_mntonname, root_path, MNAMELEN);
    //memcpy(vfsp->mnt_stat.f_mntfromname, "CODA", MNAMELEN);
    
    /* error is currently guaranteed to be zero, but in case some
       code changes... */
    CODADEBUG(1, myprintf(("coda_mount returned %d\n",error)););
    if (error)
	MARK_INT_FAIL(CODA_MOUNT_STATS);
    else
    {
	MARK_INT_SAT(CODA_MOUNT_STATS);
	//coda_instances++;
    }
    LEAVE;
    return(error);
}

int
coda_unmount(mount_t mp, int mntflags, vfs_context_t context)
{
    struct coda_mntinfo *mi = vftomi(mp);
    int active, error = 0;
    
    ENTRY;
    MARK_ENTRY(CODA_UMOUNT_STATS);
    if (!CODA_MOUNTED(mp)) {
	MARK_INT_FAIL(CODA_UMOUNT_STATS);
        LEAVE;
	return(EINVAL);
    }
    
    if (mi->mi_vfsp == mp) {	/* We found the victim */
	if (!IS_UNMOUNTING(VTOC(mi->mi_rootvp)))
        {
            LEAVE;
	    return (EBUSY); 	/* Venus is still running */
        }

#ifdef	DEBUG
	myprintf(("coda_unmount: ROOT: vp %p, cp %p\n", mi->mi_rootvp, VTOC(mi->mi_rootvp)));
#endif
	/* vnode_rele(mi->mi_rootvp); */
	active = coda_kill(mp, NOT_DOWNCALL);
	ASSERT_VOP_LOCKED(mi->mi_rootvp, "coda_unmount");
	/* mi->mi_rootvp->v_vflag &= ~VV_ROOT; */
        
	error = vflush(mi->mi_vfsp, 0, FORCECLOSE);
#ifdef CODA_VERBOSE
	myprintf(("coda_unmount: active = %d, vflush active %d\n", active, error));
#endif

	error = 0;
        vfs_setfsprivate(mp, NULL);
	/* I'm going to take this out to allow lookups to go through. I'm
	 * not sure it's important anyway. -- DCS 2/2/94
	 */
	/* vfsp->VFS_DATA = NULL; */
	/* No more vfsp's to hold onto */
	mi->mi_vfsp = NULL;
	mi->mi_rootvp = NULL;

	if (error)
	    MARK_INT_FAIL(CODA_UMOUNT_STATS);
	else
	{
	    MARK_INT_SAT(CODA_UMOUNT_STATS);
	    //coda_instances--;
	}
        LEAVE;
	return(error);
    }
    LEAVE;  
    return (EINVAL);
}

/*
 * find root of cfs
 */
int
coda_root(struct mount *mp, vnode_t *vpp, vfs_context_t vfsctx)
{
    struct coda_mntinfo *mi = vftomi(mp);
    struct vnode **result;
    int error;
    THREAD *td = CURTHREAD;    /* XXX - bnoble */
    struct proc *p = THREAD2PROC;
    CodaFid VFid;
    static const CodaFid invalfid = INVAL_FID;
 
    ENTRY;
    MARK_ENTRY(CODA_ROOT_STATS);
    result = NULL;
    
    if (mp == mi->mi_vfsp) {
	/*
	 * Cache the root across calls. We only need to pass the request
	 * on to Venus if the root vnode is the dummy we installed in
	 * coda_mount() with all c_fid members zeroed.
	 *
	 * XXX In addition, if we are called between coda_mount() and
	 * coda_start(), we assume that the request is from vfs_mount()
	 * (before the call to checkdirs()) and return the dummy root
	 * node to avoid a deadlock. This bug is fixed in the Coda CVS
	 * repository but not in any released versions as of 6 Mar 2003.
	 */
	if (mi && mi->mi_rootvp &&
            VTOC(mi->mi_rootvp) &&
	    &VTOC(mi->mi_rootvp)->c_fid &&
	    (memcmp(&VTOC(mi->mi_rootvp)->c_fid, &invalfid, sizeof(CodaFid)) != 0 || mi->mi_started == 0))
	    { /* Found valid root. */
		*vpp = mi->mi_rootvp;
		/* On Mach, this is vref.  On NetBSD, VOP_LOCK */
                //if(coda_islockedbyme(*vpp,0))
                //{
                //    myprintf(("Avoiding locking the root against myself on line %d\n",__LINE__));
                //    VOP_UNLOCK(*vpp,0,td);
                //}
	        vnode_get(*vpp);
		MARK_INT_SAT(CODA_ROOT_STATS);
                //ASSURE_LOCKED(*vpp);
                LEAVE;
		return(0);
	    }
    }

    error = venus_root(vftomi(mp), vfs_context_ucred(vfsctx), p, &VFid);

    if (!error) {
	/*
	 * Save the new rootfid in the cnode, and rehash the cnode into the
	 * cnode hash with the new fid key.
	 */
	coda_unsave(VTOC(mi->mi_rootvp));
	VTOC(mi->mi_rootvp)->c_fid = VFid;
	coda_save(VTOC(mi->mi_rootvp));

	*vpp = mi->mi_rootvp;
	vnode_get(*vpp);

	MARK_INT_SAT(CODA_ROOT_STATS);
	goto exit;
    } else if (error == ENODEV || error == EINTR) {
	/* Gross hack here! */
	/*
	 * If Venus fails to respond to the CODA_ROOT call, coda_call returns
	 * ENODEV. Return the uninitialized root vnode to allow vfs
	 * operations such as unmount to continue. Without this hack,
	 * there is no way to do an unmount if Venus dies before a 
	 * successful CODA_ROOT call is done. All vnode operations 
	 * will fail.
	 */
	*vpp = mi->mi_rootvp;
	vnode_get(*vpp);

	MARK_INT_FAIL(CODA_ROOT_STATS);
	error = 0;
	goto exit;
    } else {
	CODADEBUG( CODA_ROOT, myprintf(("error %d in CODA_ROOT\n", error)); );
	MARK_INT_FAIL(CODA_ROOT_STATS);
		
	goto exit;
    }

 exit:
   // ASSURE_LOCKED(*vpp);
    LEAVE;  
    return(error);
}

int
coda_start(mp, flags, td)
	struct mount *mp;
	int flags;
	THREAD *td;
{
	int error=0;
        ENTRY;  
	/* XXX See coda_root(). */
	vftomi(mp)->mi_started = 1;
        LEAVE;
	return (error);
}

/*
 * Get filesystem statistics.
 */
/* This routine is no longer needed */
#if 0
int
coda_nb_statfs(mount_t mp, struct vfsstatfs *sbp, THREAD *td)
{
    int error=0;
    
    struct vfsstatfs *sfsp;

    ENTRY;
/*  MARK_ENTRY(CODA_STATFS_STATS); */
    if (!CODA_MOUNTED(mp)) {
/*	MARK_INT_FAIL(CODA_STATFS_STATS);*/
	return(EINVAL);
    }
    
#if 0
    bzero(sbp, sizeof(struct statfs));
#else	
    sbp->f_type = coda_mount_type;
    sbp->f_flags = 0;
#endif
    /* XXX - what to do about f_flags, others? --bnoble */
    /* Below This is what AFS does
    	#define NB_SFS_SIZ 0x895440
     */
    /* Note: Normal fs's have a bsize of 0x400 == 1024 */
    sbp->f_bsize = 8192; /* XXX */
    sbp->f_iosize = 8192; /* XXX */
#define NB_SFS_SIZ 0x8AB75D
    sbp->f_blocks = NB_SFS_SIZ;
    sbp->f_bfree = NB_SFS_SIZ;
    sbp->f_bavail = NB_SFS_SIZ;
    sbp->f_files = 0;
    sbp->f_ffree = 0;
    sfsp=vfs_statfs(mp);

    if (sbp != sfsp)
    {
        memcpy(sbp->f_mntonname, sfsp->f_mntonname, MNAMELEN);
        memcpy(sbp->f_mntfromname, sfsp->f_mntfromname, MNAMELEN);
    }
    else
    {
#if 0
	    snprintf(sbp->f_mntonname, sizeof(sbp->f_mntonname), "/coda");
	    snprintf(sbp->f_mntfromname, sizeof(sbp->f_mntfromname), "CODA");
#endif
    }
    sbp->f_type = sfsp->vfc_typenum;
    snprintf(sbp->f_fstypename, sizeof(sbp->f_fstypename), "coda");
    bcopy((caddr_t)&(sfsp->f_fsid), (caddr_t)&(sbp->f_fsid), sizeof (fsid_t));
    MARK_INT_SAT(CODA_STATFS_STATS);
    LEAVE;
    return(0);
}
#endif /* 0 */

/*
 * Flush any pending I/O.
 */
int
coda_sync(mount_t mp, int waitfor, struct ucred *cred, THREAD *td)
{
//    ENTRY;
    MARK_ENTRY(CODA_SYNC_STATS);
    MARK_INT_SAT(CODA_SYNC_STATS);
    return(0);
}

/* 
 * fhtovp is now what vget used to be in 4.3-derived systems.  For
 * some silly reason, vget is now keyed by a 32 bit ino_t, rather than
 * a type-specific fid.  
 */
int
coda_fhtovp(mount_t mp, int fhlen, unsigned char *fhp, vnode_t *vpp, vfs_context_t vfsctx)
{
    struct cfid *cfid = (struct cfid *)fhp;
    struct cnode *cp = 0;
    int error;
    THREAD *td = CURTHREAD; /* XXX -mach */
    struct proc *p = THREAD2PROC;
    CodaFid VFid;
    int vtype;

    ENTRY;
    
    MARK_ENTRY(CODA_VGET_STATS);
    /* Check for vget of control object. */
    if (IS_CTL_FID(&cfid->cfid_fid)) {
	*vpp = coda_ctlvp;
	vnode_ref(coda_ctlvp);
	MARK_INT_SAT(CODA_VGET_STATS);
	return(0);
    }
    
    error = venus_fhtovp(vftomi(mp), &cfid->cfid_fid,  vfs_context_ucred(vfsctx), p, &VFid, &vtype);
    
    if (error) {
	CODADEBUG(CODA_VGET, myprintf(("vget error %d\n",error));)
	    *vpp = (struct vnode *)0;
    } else {
	CODADEBUG(CODA_VGET, myprintf(("vget: %s type %d result %d\n", coda_f2s(&VFid), vtype, error)); )	    
	cp = make_coda_node(&VFid, mp, vtype);
        if(cp==0) return ENOMEM;

	*vpp = CTOV(cp);
    }
    //ASSURE_LOCKED(*vpp);
    return(error);
}

/*
 * To allow for greater ease of use, some vnodes may be orphaned when
 * Venus dies.  Certain operations should still be allowed to go
 * through, but without propagating ophan-ness.  So this function will
 * get a new vnode for the file from the current run of Venus.  */
 
#if 0
int
getNewVnode(vpp)
     struct vnode **vpp;
{
    struct cfid cfid;
    struct coda_mntinfo *mi = vftomi(vnode_mount(*vpp));
    
    ENTRY;

    cfid.cfid_len = (short)sizeof(CodaFid);
    cfid.cfid_fid = VTOC(*vpp)->c_fid;	/* Structure assignment. */
    /* XXX ? */

    /* We're guessing that if set, the 1st element on the list is a
     * valid vnode to use. If not, return ENODEV as venus is dead.
     */
    if (mi->mi_vfsp == NULL)
	return ENODEV;
    
    return coda_fhtovp(mi->mi_vfsp, 0,(struct fid*)&cfid, NULL, vpp,
		      NULL, NULL);
}
#endif /* 0 */
   
struct mount *devtomp(dev)
/* get the mount structure corresponding to a given device.  Assume 
 * device corresponds to a UFS. Return NULL if no device is found.
 */

    dev_t dev;
{
    struct mount *mp;
    
 //    int nmounts=mount_getvfscnt();
    
   
#ifdef DARWIN
    TAILQ_FOREACH(mp, &mountlist, mnt_list) {
        if (hfsdev(mp) == dev ) {
#else
    TAILQ_FOREACH(mp, &mountlist, mnt_list) {
	if (((VFSTOUFS(mp))->um_dev == dev)) {
#endif

	    /* mount corresponds to UFS and the device matches one we want */
	    return(mp); 
	}
    }
    /* mount structure wasn't found */ 
    return(NULL); 
}

struct vfsops coda_vfsops = {
    .vfs_mount =		coda_mount,
    .vfs_root = 		coda_root,
    .vfs_start =		coda_start,
 /*   .vfs_statfs =		coda_nb_statfs, */
    .vfs_sync = 		coda_sync,
    .vfs_unmount =		coda_unmount,
    .vfs_fhtovp =		coda_fhtovp,
};

VFS_SET(coda_vfsops, coda, VFCF_NETWORK);
