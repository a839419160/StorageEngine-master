#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>


//flag = 0  屏蔽信号
//       1  解除屏蔽
int block_allsig(int flag)
{
    sigset_t    signal_set;

    if (sigemptyset(&signal_set) < 0) 
    { 
        return -1; 
    }

    if (sigfillset(&signal_set) < 0) 
    { 
        return -1; 
    }

    if (flag == 0)
    {
        if (pthread_sigmask(SIG_BLOCK,&signal_set,NULL) != 0) { return -1;}
    }
    else 
    {
        if (pthread_sigmask(SIG_UNBLOCK,&signal_set,NULL) != 0) { return -1;}
    }

    return 0;
}

/*
int reg_signal(int signo,sig_bc func,void *data)
{
    struct sigaction sig_act;
    
    if (sigemptyset(&sig_act.sa_mask) < 0)
    {
        return -1;
    }
    sig_act.sa_flags = SA_SIGINFO;
    sig_act.sa_sigaction = func;

    if(sigaction(signo,&sig_act,NULL))
    {
        return -1;
    }
    return 0;
}
*/
