/*
 * 
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
 * 	@(#) src/sys/cfs/coda_vfsops.h,v 1.1.1.1 1998/08/29 21:14:52 rvb Exp $ 
 * $FreeBSD: src/sys/coda/coda_vfsops.h,v 1.8 2003/09/07 07:43:09 tjr Exp $
 * 
 */

/*
 * cfid structure:
 * This overlays the fid structure (see vfs.h)
 * Only used below and will probably go away.
 */

struct cfid {
    u_short	cfid_len;
    u_short     padding;
    CodaFid	cfid_fid;
};

struct mbuf;
struct mount;

int coda_vfsopstats_init(void);
int coda_fhtovp(struct mount *, struct fid *, struct mbuf *, struct vnode **,
                      int *, struct ucred **);

#ifdef DARWIN
typedef int vfs_mount_t(struct mount *mp, char *path, caddr_t data, struct nameidata *ndp, struct proc *p);
typedef int vfs_start_t(struct mount *mp, int flags, struct proc *p);
typedef int vfs_unmount_t(struct mount *mp, int mntflags, struct proc *p);
typedef int vfs_root_t(struct mount *mp, struct vnode **vpp);
typedef int vfs_quotactl_t(struct mount *mp, int cmds, uid_t uid,caddr_t arg, struct proc *p);
typedef int vfs_statfs_t(struct mount *mp, struct statfs *sbp, struct proc *p);
typedef int vfs_sync_t(struct mount *mp, int waitfor, struct ucred *cred, struct proc *p);
typedef int vfs_vget_t(struct mount *mp, void *ino, struct vnode **vpp);
typedef int vfs_fhtovp_t(struct mount *mp, struct fid *fhp, struct mbuf *nam, struct vnode **vpp);
typedef int vfs_vptofh_t(struct vnode *vp, struct fid *fhp);
typedef int vfs_init_t(struct vfsconf *);

#else /* !DARWIN */
vfs_mount_t	coda_mount;
vfs_start_t	coda_start;
vfs_unmount_t	coda_unmount;
vfs_root_t	coda_root;
vfs_quotactl_t	coda_quotactl;
vfs_statfs_t	coda_nb_statfs;
vfs_sync_t	coda_sync;
vfs_vget_t	coda_vget;
vfs_vptofh_t	coda_vptofh;
vfs_init_t	coda_init;
#endif /* !DARWIN */

int getNewVnode(struct vnode **vpp);
