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

#include <vcoda.h>

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

MALLOC_DEFINE(M_CODA, "CODA storage", "Various Coda Structures");

/* long long codadebug = CODADBGMSK(CODA_LOOKUP) | CODADBGMSK(CODA_SETATTR); */
long long codadebug = 0;
int coda_vfsop_print_entry = 0;
#define ENTRY    if(coda_vfsop_print_entry) PRINTENTRY
#define LEAVE    if(coda_vfsop_print_entry) PRINTLEAVE

struct vnode *coda_ctlvp;
struct coda_mntinfo coda_mnttbl[NVCODA]; /* indexed by minor device number */

/* structure to keep statistics of internally generated/satisfied calls */

struct coda_op_stats coda_vfsopstats[CODA_VFSOPS_SIZE];

#define MARK_ENTRY(op) (coda_vfsopstats[op].entries++)
#define MARK_INT_SAT(op) (coda_vfsopstats[op].sat_intrn++)
#define MARK_INT_FAIL(op) (coda_vfsopstats[op].unsat_intrn++)
#define MRAK_INT_GEN(op) (coda_vfsopstats[op].gen_intrn++)

extern int coda_nc_initialized;     /* Set if cache has been initialized */
extern int vc_nb_open(dev_t, int, int, THREAD *);
extern int coda_instances;
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

int
coda_init(struct vfsconf * vfsp)
{
    ENTRY;
    coda_mount_type = vfsp->vfc_typenum; 
    LEAVE;
    return 0;
}

/*
 * cfs mount vfsop
 * Set up mount info record and attach it to vfs struct.
 */
/*ARGSUSED*/
int
coda_mount(vfsp, path, data, ndp, td)
    struct mount *vfsp;		/* Allocated and initialized by mount(2) */
    char *path;			/* path covered: ignored by the fs-layer */
    caddr_t data;		/* Need to define a data type for this in netbsd? */
    struct nameidata *ndp;	/* Clobber this to lookup the device name */
    THREAD *td;
{
    struct vnode *dvp;
    struct cnode *cp;
    char root_path[MNAMELEN];
    size_t len;
    dev_t dev;
    struct coda_mntinfo *mi;
    struct vnode *rootvp;
    CodaFid rootfid = INVAL_FID;
    CodaFid ctlfid = CTL_FID;
    int error;

    ENTRY;

    coda_vfsopstats_init();
    coda_vnodeopstats_init();
    
    MARK_ENTRY(CODA_MOUNT_STATS);
    if (CODA_MOUNTED(vfsp)) {
	MARK_INT_FAIL(CODA_MOUNT_STATS);
        LEAVE;
	return(EBUSY);
    }

    /* Get mount point name */
    error = copyinstr(path, root_path, MNAMELEN-1, &len);
    if (error)
	    return(error);
    memset(root_path + len, '\0', MNAMELEN - len);

    /* Validate mount device.  Similar to getmdev(). */
    NDINIT(ndp, LOOKUP, FOLLOW, UIO_USERSPACE, data, td);
    error = namei(ndp);
    dvp = ndp->ni_vp;

    if (error) {
	MARK_INT_FAIL(CODA_MOUNT_STATS);
        LEAVE;
	return (error);
    }
    if (dvp->v_type != VCHR) {
	MARK_INT_FAIL(CODA_MOUNT_STATS);
	vrele(dvp);
	NDFREE(ndp, NDF_ONLY_PNBUF);
        LEAVE;
	return(ENXIO);
    }
    dev = dvp->v_rdev;
    vrele(dvp);
    NDFREE(ndp, NDF_ONLY_PNBUF);

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
    vfsp->mnt_data = (qaddr_t)mi;
    vfs_getnewfsid (vfsp);

    mi->mi_vfsp = vfsp;
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
    cp = make_coda_node(&rootfid, vfsp, VDIR);
    if(cp==0) 
    {
        LEAVE;
        return ENOMEM;
    }
    SET_CNODE_NAME(root_path);
    rootvp = CTOV(cp);
    rootvp->v_vflag |= VV_ROOT;
	
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

    /* Add vfs and rootvp to chain of vfs hanging off mntinfo */
    mi->mi_vfsp = vfsp;
    mi->mi_rootvp = rootvp;
    
    /* set filesystem block size */
    vfsp->mnt_stat.f_bsize = 8192;	    /* XXX -JJK */

    /* Set f_iosize.  XXX -- inamura@isl.ntt.co.jp. 
       For vnode_pager_haspage() references. The value should be obtained 
       from underlying UFS. */
    /* Checked UFS. iosize is set as 8192 */
    vfsp->mnt_stat.f_iosize = 8192;

    memcpy(vfsp->mnt_stat.f_mntonname, root_path, MNAMELEN);
    memcpy(vfsp->mnt_stat.f_mntfromname, "CODA", MNAMELEN);

    /* error is currently guaranteed to be zero, but in case some
       code changes... */
    CODADEBUG(1, myprintf(("coda_mount returned %d\n",error)););
    if (error)
	MARK_INT_FAIL(CODA_MOUNT_STATS);
    else
    {
	MARK_INT_SAT(CODA_MOUNT_STATS);
	coda_instances++;
    }
    LEAVE;
    return(error);
}

int
coda_unmount(vfsp, mntflags, td)
    struct mount *vfsp;
    int mntflags;
    THREAD *td;
{
    struct coda_mntinfo *mi = vftomi(vfsp);
    int active, error = 0;
    
    ENTRY;
    MARK_ENTRY(CODA_UMOUNT_STATS);
    if (!CODA_MOUNTED(vfsp)) {
	MARK_INT_FAIL(CODA_UMOUNT_STATS);
        LEAVE;
	return(EINVAL);
    }
    
    if (mi->mi_vfsp == vfsp) {	/* We found the victim */
	if (!IS_UNMOUNTING(VTOC(mi->mi_rootvp)))
        {
            LEAVE;
	    return (EBUSY); 	/* Venus is still running */
        }

#ifdef	DEBUG
	myprintf(("coda_unmount: ROOT: vp %p, cp %p\n", mi->mi_rootvp, VTOC(mi->mi_rootvp)));
#endif
	vrele(mi->mi_rootvp);
	active = coda_kill(vfsp, NOT_DOWNCALL);
	ASSERT_VOP_LOCKED(mi->mi_rootvp, "coda_unmount");
	mi->mi_rootvp->v_vflag &= ~VV_ROOT;
	error = vflush(mi->mi_vfsp, 0, FORCECLOSE);
#ifdef CODA_VERBOSE
	myprintf(("coda_unmount: active = %d, vflush active %d\n", active, error));
#endif
	error = 0;
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
	    coda_instances--;
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
coda_root(vfsp, vpp)
	struct mount *vfsp;
	struct vnode **vpp;
{
    struct coda_mntinfo *mi = vftomi(vfsp);
    struct vnode **result;
    int error;
    THREAD *td = CURTHREAD;    /* XXX - bnoble */
    struct proc *p = THREAD2PROC;
    CodaFid VFid;
    static const CodaFid invalfid = INVAL_FID;
 
    ENTRY;
    MARK_ENTRY(CODA_ROOT_STATS);
    result = NULL;
    
    if (vfsp == mi->mi_vfsp) {
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
	    memcmp(&VTOC(mi->mi_rootvp)->c_fid, &invalfid,
	    sizeof(CodaFid)) != 0 || mi->mi_started == 0)
	    { /* Found valid root. */
		*vpp = mi->mi_rootvp;
		/* On Mach, this is vref.  On NetBSD, VOP_LOCK */
                if(VOP_ISLOCKED(*vpp))
                {
                    myprintf(("Avoiding locking the root against myself on line %d\n",__LINE__));
                    vref(*vpp);
                }
                else
		{ 
		    vget(*vpp, LK_EXCLUSIVE, td); 
		}
		MARK_INT_SAT(CODA_ROOT_STATS);
                ASSURE_LOCKED(*vpp);
                LEAVE;
		return(0);
	    }
    }

    error = venus_root(vftomi(vfsp), td->td_ucred, p, &VFid);

    if (!error) {
	/*
	 * Save the new rootfid in the cnode, and rehash the cnode into the
	 * cnode hash with the new fid key.
	 */
	coda_unsave(VTOC(mi->mi_rootvp));
	VTOC(mi->mi_rootvp)->c_fid = VFid;
	coda_save(VTOC(mi->mi_rootvp));

	*vpp = mi->mi_rootvp;
#ifndef USE_VGET
        if(VOP_ISLOCKED(*vpp))
        {
            myprintf(("Avoiding locking the root against myself on line %d\n",__LINE__));
        }
        else
        {
            vref(*vpp);
            vn_lock(*vpp, LK_EXCLUSIVE, td);
        }
#else
	vget(*vpp, LK_EXCLUSIVE, td);
#endif

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
#ifndef USE_VGET
        if(VOP_ISLOCKED(*vpp))
        {
            myprintf(("Avoiding locking the root against myself on line %d\n",__LINE__));
        }
        else
        {
            vref(*vpp);
            vn_lock(*vpp, LK_EXCLUSIVE, td);
        }
#else
	vget(*vpp, LK_EXCLUSIVE, td);
#endif

	MARK_INT_FAIL(CODA_ROOT_STATS);
	error = 0;
	goto exit;
    } else {
	CODADEBUG( CODA_ROOT, myprintf(("error %d in CODA_ROOT\n", error)); );
	MARK_INT_FAIL(CODA_ROOT_STATS);
		
	goto exit;
    }

 exit:
    ASSURE_LOCKED(*vpp);
    LEAVE;  
    return(error);
}

int
coda_start(mp, flags, td)
	struct mount *mp;
	int flags;
	THREAD *td;
{
        ENTRY;  
	/* XXX See coda_root(). */
	vftomi(mp)->mi_started = 1;
        LEAVE;
	return (0);
}

/*
 * Get filesystem statistics.
 */
int
coda_nb_statfs(vfsp, sbp, td)
    register struct mount *vfsp;
    struct statfs *sbp;
    THREAD *td;
{
    ENTRY;
/*  MARK_ENTRY(CODA_STATFS_STATS); */
    if (!CODA_MOUNTED(vfsp)) {
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
    if (sbp != &vfsp->mnt_stat)
    {
        memcpy(sbp->f_mntonname, vfsp->mnt_stat.f_mntonname, MNAMELEN);
        memcpy(sbp->f_mntfromname, vfsp->mnt_stat.f_mntfromname, MNAMELEN);
    }
    else
    {
#if 0
	    snprintf(sbp->f_mntonname, sizeof(sbp->f_mntonname), "/coda");
	    snprintf(sbp->f_mntfromname, sizeof(sbp->f_mntfromname), "CODA");
#endif
    }
    sbp->f_type = vfsp->mnt_vfc->vfc_typenum;
    snprintf(sbp->f_fstypename, sizeof(sbp->f_fstypename), "coda");
    bcopy((caddr_t)&(vfsp->mnt_stat.f_fsid), (caddr_t)&(sbp->f_fsid), sizeof (fsid_t));
    MARK_INT_SAT(CODA_STATFS_STATS);
    LEAVE;
    return(0);
}

/*
 * Flush any pending I/O.
 */
int
coda_sync(vfsp, waitfor, cred, td)
    struct mount *vfsp;
    int    waitfor;
    struct ucred *cred;
    THREAD *td;
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
coda_fhtovp(vfsp, fhp, nam, vpp, exflagsp, creadanonp)
    register struct mount *vfsp;    
    struct fid *fhp;
    struct mbuf *nam;
    struct vnode **vpp;
    int *exflagsp;
    struct ucred **creadanonp;
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
	vref(coda_ctlvp);
	MARK_INT_SAT(CODA_VGET_STATS);
	return(0);
    }
    
    error = venus_fhtovp(vftomi(vfsp), &cfid->cfid_fid, td->td_ucred, p, &VFid, &vtype);
    
    if (error) {
	CODADEBUG(CODA_VGET, myprintf(("vget error %d\n",error));)
	    *vpp = (struct vnode *)0;
    } else {
	CODADEBUG(CODA_VGET, myprintf(("vget: %s type %d result %d\n", coda_f2s(&VFid), vtype, error)); )	    
	cp = make_coda_node(&VFid, vfsp, vtype);
        if(cp==0) return ENOMEM;

	*vpp = CTOV(cp);
    }
    ASSURE_LOCKED(*vpp);
    return(error);
}

/*
 * To allow for greater ease of use, some vnodes may be orphaned when
 * Venus dies.  Certain operations should still be allowed to go
 * through, but without propagating ophan-ness.  So this function will
 * get a new vnode for the file from the current run of Venus.  */
 
int
getNewVnode(vpp)
     struct vnode **vpp;
{
    struct cfid cfid;
    struct coda_mntinfo *mi = vftomi((*vpp)->v_mount);
    
    ENTRY;

    cfid.cfid_len = (short)sizeof(CodaFid);
    cfid.cfid_fid = VTOC(*vpp)->c_fid;	/* Structure assignment. */
    /* XXX ? */

    /* We're guessing that if set, the 1st element on the list is a
     * valid vnode to use. If not, return ENODEV as venus is dead.
     */
    if (mi->mi_vfsp == NULL)
	return ENODEV;
    
    return coda_fhtovp(mi->mi_vfsp, (struct fid*)&cfid, NULL, vpp,
		      NULL, NULL);
}

#ifndef  DARWIN
#include <ufs/ufs/extattr.h>
#include <ufs/ufs/quota.h>
#include <ufs/ufs/ufsmount.h>
#endif /* !DARWIN */
/* get the mount structure corresponding to a given device.  Assume 
 * device corresponds to a UFS. Return NULL if no device is found.
 */ 
struct mount *devtomp(dev)
    dev_t dev;
{
    struct mount *mp;
   
#ifdef DARWIN
    CIRCLEQ_FOREACH(mp, &mountlist, mnt_list) {
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
    .vfs_statfs =		coda_nb_statfs,
    .vfs_sync = 		coda_sync,
    .vfs_unmount =		coda_unmount,
    .vfs_fhtovp =		coda_fhtovp,
};

VFS_SET(coda_vfsops, coda, VFCF_NETWORK);
