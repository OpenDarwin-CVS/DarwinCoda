/*
 *  uglyhack.c
 *  coda_kext
 *
 *  Created by Christer BernŽrus on 2006-06-09.
 *  Copyright 2006 __MyCompanyName__. All rights reserved.
 *
 */

#include "uglyhack.h"


int ugly_proc_iscore(struct uglyhack_proc *p)
{
    return p->p_acflag & UGLY_ACORE;
}


pid_t ugly_proc_pgid(struct proc *p)
{
    struct uglyhack_proc *up=(struct uglyhack_proc*)p;
    struct uglyhack_pgrp *pgp;
    
    pgp=up->p_pgrp;
    
    return pgp->pg_id;
}
