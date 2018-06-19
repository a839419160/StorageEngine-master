#ifndef __SL_SIG_H__
#define __SL_SIG_H__
#include <signal.h>

#define MASK_SIG    SIG_BLOCK
#define UNMASK_SIG  SIG_UNBLOCK

#define SIG_RETURN  (SIGRTMIN+4)

typedef struct __sthr_info_t
{
    int thr_id;
    int cpu_id;
}sthr_info_t;

sthr_info_t *sthr_info;

int block_allsig(int flag);

#endif
