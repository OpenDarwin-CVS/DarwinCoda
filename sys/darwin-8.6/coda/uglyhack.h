/* UglyHack(TM) - Ugly hacks to cope with the incompleteness of the Tiger KPI
 *
 * An UglyHack is a renamed copy of something that is meant to be opaque to me as a kext programmer,
 * but because of incpmpleteness of the KPI some things need to be accesses using structures deliberately
 * copied from the *_internal.h sources. 
 *
 * Using these hacks is a sure way to create maintenance problems. To make it easier to spot things to
 * maintain, the structures are renamed by adding an uglyhack_ prefix to them. To actually use them,
 * the struct pointers reed to be recasted to the uglyhack_ version, thus leaving traces in the source 
 * code as to where these hacks are used, and may have to be revised as new version show up or be deleted
 * if suitable KPI's are released.
 */
#ifndef UGLYHACK_H
#define UGLYHACK_H
#include <sys/event.h>
#include <machine/locks.h>


/* 
* One structure allocated per process group.
 */
struct  uglyhack_pgrp {
    LIST_ENTRY(pgrp) pg_hash;       /* Hash chain. */
    LIST_HEAD(, proc) pg_members;   /* Pointer to pgrp members. */
    struct  session *pg_session;    /* Pointer to session. */
    pid_t   pg_id;                  /* Pgrp id. */ 
    int     pg_jobc;        /* # procs qualifying pgrp for job control */
};      

/* The proc structure */
/* The SLIST_HEAD definition below has the side effect of defining struct klist, which is what we want */
SLIST_HEAD(klist, ugly_knote);

typedef struct {
    unsigned int            opaque[3];
} ugly_lck_mtx_t;

typedef struct {
    unsigned int            opaque[3];
} ugly_lck_rw_t;

struct  uglyhack_proc {
        LIST_ENTRY(proc) p_list;        /* List of all processes. */

        /* substructures: */
        struct  ucred *p_ucred;         /* Process owner's identity. */
        struct  filedesc *p_fd;         /* Ptr to open files structure. */
        struct  pstats *p_stats;        /* Accounting/statistics (PROC ONLY). */
        struct  plimit *p_limit;        /* Process limits. */
        struct  sigacts *p_sigacts;     /* Signal actions, state (PROC ONLY). */

#define uglyhack_p_rlimit        p_limit->pl_rlimit

        int     p_flag;                 /* P_* flags. */
        char    p_stat;                 /* S* process status. */
        char    p_shutdownstate;
        char    p_pad1[2];

        pid_t   p_pid;                  /* Process identifier. */
        LIST_ENTRY(proc) p_pglist;      /* List of processes in pgrp. */
        struct  proc *p_pptr;           /* Pointer to parent process. */
        LIST_ENTRY(proc) p_sibling;     /* List of sibling processes. */
        LIST_HEAD(, proc) p_children;   /* Pointer to list of children. */

/* The following fields are all zeroed upon creation in fork. */
#define uglyhack_p_startzero     p_oppid

        pid_t   p_oppid;         /* Save parent pid during ptrace. XXX */
        int     p_dupfd;         /* Sideways return value from fdopen. XXX */

        /* scheduling */
        u_int   p_estcpu;        /* Time averaged value of p_cpticks. */
        int     p_cpticks;       /* Ticks of cpu time. */
        fixpt_t p_pctcpu;        /* %cpu for this process during p_swtime */
        void    *p_wchan;        /* Sleep address. */
        char    *p_wmesg;        /* Reason for sleep. */
        u_int   p_swtime;        /* DEPRECATED (Time swapped in or out.) */
#define uglyhack_p_argslen p_swtime       /* Length of process arguments. */
        u_int   p_slptime;       /* Time since last blocked. */

        struct  itimerval p_realtimer;  /* Alarm timer. */
        struct  timeval p_rtime;        /* Real time. */
        u_quad_t p_uticks;              /* Statclock hits in user mode. */
        u_quad_t p_sticks;              /* Statclock hits in system mode. */
        u_quad_t p_iticks;              /* Statclock hits processing intr. */

        int     p_traceflag;            /* Kernel trace points. */
        struct  vnode *p_tracep;        /* Trace to vnode. */

        sigset_t p_siglist;             /* DEPRECATED. */

        struct  vnode *p_textvp;        /* Vnode of executable. */

/* End area that is zeroed on creation. */
#define uglyhack_p_endzero       p_hash.le_next

        /*
         * Not copied, not zero'ed.
         * Belongs after p_pid, but here to avoid shifting proc elements.
         */
        LIST_ENTRY(proc) p_hash;        /* Hash chain. */
        TAILQ_HEAD( ,eventqelt) p_evlist;

/* The following fields are all copied upon creation in fork. */
#define uglyhack_p_startcopy     p_sigmask

        sigset_t p_sigmask;             /* DEPRECATED */
        sigset_t p_sigignore;   /* Signals being ignored. */
        sigset_t p_sigcatch;    /* Signals being caught by user. */

        u_char  p_priority;     /* Process priority. */
        u_char  p_usrpri;       /* User-priority based on p_cpu and p_nice. */
        char    p_nice;         /* Process "nice" value. */
        char    p_comm[MAXCOMLEN+1];

        struct  uglyhack_pgrp *p_pgrp;   /* Pointer to process group. */

/* End area that is copied on creation. */
#define uglyhack_p_endcopy       p_xstat

        u_short p_xstat;        /* Exit status for wait; also stop signal. */
        u_short p_acflag;       /* Accounting flags. */
        struct  rusage *p_ru;   /* Exit information. XXX */

        int     p_debugger;     /* 1: can exec set-bit programs if suser */

        void    *task;                  /* corresponding task */
        void    *sigwait_thread;        /* 'thread' holding sigwait */
        char    signal_lock[72];
        boolean_t        sigwait;       /* indication to suspend */
        void    *exit_thread;           /* Which thread is exiting? */
        user_addr_t user_stack;         /* where user stack was allocated */
        void * exitarg;                 /* exit arg for proc terminate */
        void * vm_shm;                  /* for sysV shared memory */
        int  p_argc;                    /* saved argc for sysctl_procargs() */
        int             p_vforkcnt;             /* number of outstanding vforks */
    void *  p_vforkact;     /* activation running this vfork proc */
        TAILQ_HEAD( , uthread) p_uthlist; /* List of uthreads  */
        /* Following fields are info from SIGCHLD */
        pid_t   si_pid;
        u_short si_status;
        u_short si_code;
        uid_t   si_uid;
        TAILQ_HEAD( , aio_workq_entry ) aio_activeq; /* active async IO requests */
        int             aio_active_count;       /* entries on aio_activeq */
        TAILQ_HEAD( , aio_workq_entry ) aio_doneq;       /* completed async IO requests */
        int             aio_done_count;         /* entries on aio_doneq */

        struct klist p_klist;  /* knote list */
        ugly_lck_mtx_t       p_mlock;        /* proc lock to protect evques */
        ugly_lck_mtx_t       p_fdmlock;      /* proc lock to protect evques */
        unsigned int p_fdlock_pc[4];
        unsigned int p_fdunlock_pc[4];
        int             p_fpdrainwait;
        unsigned int            p_lflag;                /* local flags */
        unsigned int            p_ladvflag;             /* local adv flags*/
        unsigned int            p_internalref;  /* temp refcount field */
#if DIAGNOSTIC
#if SIGNAL_DEBUG
        unsigned int lockpc[8];
        unsigned int unlockpc[8];
#endif /* SIGNAL_DEBUG */
#endif /* DIAGNOSTIC */
};

#define UGLY_ACORE   0x08            /* dumped core */


int ugly_proc_iscore(struct uglyhack_proc *p);
pid_t ugly_proc_pgid(struct proc *p);

/* Uglyhacks from mount_internal */

TAILQ_HEAD(vnodelst, vnode);

struct mount {
    TAILQ_ENTRY(mount) mnt_list;            /* mount list */
    int32_t         mnt_count;              /* reference on the mount */
    ugly_lck_mtx_t       mnt_mlock;              /* mutex that protects mount point */
    struct vfsops   *mnt_op;                /* operations on fs */
    struct vfstable *mnt_vtable;            /* configuration info */
    struct vnode    *mnt_vnodecovered;      /* vnode we mounted on */
    struct vnodelst mnt_vnodelist;          /* list of vnodes this mount */
    struct vnodelst mnt_workerqueue;                /* list of vnodes this mount */
    struct vnodelst mnt_newvnodes;          /* list of vnodes this mount */
    int             mnt_flag;               /* flags */
    int             mnt_kern_flag;          /* kernel only flags */
    int             mnt_lflag;                      /* mount life cycle flags */
    int             mnt_maxsymlinklen;      /* max size of short symlink */
    struct vfsstatfs        mnt_vfsstat;            /* cache of filesystem stats */
    qaddr_t         mnt_data;               /* private data */ 
    /* Cached values of the IO constraints for the device */
    u_int32_t       mnt_maxreadcnt;         /* Max. byte count for read */
    u_int32_t       mnt_maxwritecnt;        /* Max. byte count for write */
    u_int32_t       mnt_segreadcnt;         /* Max. segment count for read */
    u_int32_t       mnt_segwritecnt;        /* Max. segment count for write */
    u_int32_t       mnt_maxsegreadsize;     /* Max. segment read size  */
    u_int32_t       mnt_maxsegwritesize;    /* Max. segment write size */
    u_int32_t       mnt_devblocksize;       /* the underlying device block size */
    ugly_lck_rw_t        mnt_rwlock;             /* mutex readwrite lock */
    ugly_lck_mtx_t       mnt_renamelock;         /* mutex that serializes renames that change shape of tree */
    vnode_t         mnt_devvp;              /* the device mounted on for local file systems */
    int32_t         mnt_crossref;           /* refernces to cover lookups  crossing into mp */
    int32_t         mnt_iterref;            /* refernces to cover iterations; drained makes it -ve  */
    
    /* XXX 3762912 hack to support HFS filesystem 'owner' */
    uid_t           mnt_fsowner;
    gid_t           mnt_fsgroup;
};

extern TAILQ_HEAD(mntlist, mount) mountlist;

/*
 * The vnodeop_desc structure is here because the kpi wont give me access to vdesc_name
 */
/* 
* This structure describes the vnode operation taking place.
 */
struct ugly_vnodeop_desc {
    int     vdesc_offset;           /* offset in vector--first for speed */
    char    *vdesc_name;            /* a readable name for debugging */
    int     vdesc_flags;            /* VDESC_* flags */
    
    /*
     * These ops are used by bypass routines to map and locate arguments.
     * Creds and procs are not needed in bypass routines, but sometimes
     * they are useful to (for example) transport layers. 
     * Nameidata is useful because it has a cred in it.
     */
    int     *vdesc_vp_offsets;      /* list ended by VDESC_NO_OFFSET */
    int     vdesc_vpp_offset;       /* return vpp location */
    int     vdesc_cred_offset;      /* cred location, if any */
    int     vdesc_proc_offset;      /* proc location, if any */
    int     vdesc_componentname_offset; /* if any */ 
    int     vdesc_context_offset;   /* context location, if any */
    /* 
        * Finally, we've got a list of private data (about each operation)
     * for each transport layer.  (Support to manage this list is not
                                   * yet part of BSD.)
     */
    caddr_t *vdesc_transports;
};


#endif /* UGLYHACK_H */
