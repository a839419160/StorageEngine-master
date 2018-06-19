#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <pthread.h>
#include <time.h>
#include <aio.h>
#include "sl_write.h"
#include "sl_log.h"
#include "sl_read.h"
#include "sl_com.h"
#include "sl_sig.h"


//创建新的文件，并一一对磁盘信息进行更新
int change_file(disk_info_t *d_info)
{
    time_t t;
    char buff[PATH_MAX] = {0};
    if (d_info->file_fd > 0)
    {
        close(d_info->file_fd);
    }
    t = time(NULL);
    sprintf(buff,"%s/%ld.dat", d_info->path,t);
    while((d_info->file_fd = open(buff,O_CREAT | O_WRONLY)) < 0)
    {
        usleep(10);
        continue;
    }

    d_info->cur_fsize = 0;
    d_info->cur_ftime = (int64_t)t;
    

    return 0;

}

void * thw_run(void *args)
{
    wthr_info_t *w_info = NULL;
    int i = 0,disk_id = 0;
    time_t t;

    //DBG("INTO thw_run");
    w_info = (wthr_info_t *)args;

   //DBG("w disk_num:[%d]",w_info->disk_num);
    while(1)
    {
        t = time(NULL);
        for(i = 0; i < w_info->disk_num;++i)
        {
            disk_id = w_info->disk_id[i];
            //当前磁盘正在有磁盘正在写入，当前不能继续写入，应该等待正在写入的数据写入完成
            DBG("----------------");
            if (disk_info[disk_id].w_flag == 0)
            {
                continue;
            }
            //循环获取每个磁盘数据队列中的数据
            if (!read_outval(disk_info[disk_id].bbuff,node_info_t,disk_info[disk_id].node_info)) 
            {
                DBG("disk:[%d] have not data",disk_id);
                continue;
            }

            //如果获取的地址为空，则表示队列有问题，这时候程序应该退出
            if (disk_info[disk_id].node_info == NULL) 
            { 
                ERR("get node is null, disk_id is[%d]",disk_id);
                exit(0);
            }


            char buff[32] = {0};
            strncpy(buff,disk_info[disk_id].node_info->buff,31);
            DBG("get data:[%10s]",buff);
            //对文件大小或者文件间隔时间进行判断，如果超过标准则创建新文件
            if (disk_info[disk_id].seg_type == 0)//time
            {
                if((t - disk_info[disk_id].cur_ftime) > disk_info[disk_id].time_temp)
                {
                    change_file(&disk_info[disk_id]);
                }
                else
                {
                    if (disk_info[disk_id].cur_fsize > disk_info[disk_id].file_size)
                    {
                        change_file(&disk_info[disk_id]);
                    }
                }
                //指定aio要往哪个文件描述符里写入数据
                disk_info[disk_id].my_aiocb->aio_fildes = disk_info[disk_id].file_fd;
                //对要写入的数据地址进行赋值
                disk_info[disk_id].my_aiocb->aio_buf = disk_info[disk_id].node_info->buff;
                //要写入的数据长度
                disk_info[disk_id].my_aiocb->aio_nbytes = disk_info[disk_id].node_info->len;
                //写入文件的文件偏移量
                disk_info[disk_id].my_aiocb->aio_offset = disk_info[disk_id].cur_fsize;
                //使用信号通知进程，系统已经完成数据的写入过程
                disk_info[disk_id].my_aiocb->aio_sigevent.sigev_notify = SIGEV_SIGNAL;
                //使用SIG_RETURN这个自定义信号来通知
                disk_info[disk_id].my_aiocb->aio_sigevent.sigev_signo = SIG_RETURN;
                //使用信号通知进程事件触发的时间，顺便携带一个参数过去。
                disk_info[disk_id].my_aiocb->aio_sigevent.sigev_value.sival_ptr = &disk_info[disk_id];

                //开始写
                while(aio_write(disk_info[disk_id].my_aiocb) < 0) 
                {
                    usleep(10);
                    continue;
                }

                disk_info[disk_id].w_flag = 0;

           //     while(!write_inval(disk_info[disk_id].fbuff,node_info_t,disk_info[disk_id].node_info)) { continue;}
            }

        }
    }
    return NULL;
}

/*
 *信号处理：
    1.判断数据是否写入成功
    2.成功则将node_info写入到空闲队列中
    3.失败则要重新写入数据
 *
 *
 */


void sig_write(int signo,siginfo_t *info,void *ctext)
{
    disk_info_t *d_info;
    DBG("get signal:[%d]",signo);
    if(signo == SIG_RETURN)
    {
        d_info = (disk_info_t*)info->si_ptr;
        if (aio_error(d_info->my_aiocb) == 0)
        {
            //写入成功
            if(d_info->my_aiocb->aio_nbytes != aio_return(d_info->my_aiocb))
            {
                //写入数据不完全
                while(aio_write(d_info->my_aiocb) < 0 )
                {
                    usleep(10);
                    continue;
                }
            }
            else
            {
                while(!write_inval(d_info->fbuff,node_info_t,d_info->node_info))
                {
                    continue;
                }
                d_info->w_flag = 1;
            }
        }
        else
        {
            while(aio_write(d_info->my_aiocb) < 0 )
            {
                usleep(10);
                continue;
            }

        }

    }
    return ;
}

void* ths_run(void *args)
{
    struct sigaction sig_act;
    DBG("INTO ths_run");
    


    if (sigemptyset(&sig_act.sa_mask) < 0)
    {
        return NULL;
    }

    sig_act.sa_flags = SA_SIGINFO;
    sig_act.sa_sigaction = sig_write;

    if(sigaction(SIG_RETURN,&sig_act,NULL))
    {
        return NULL;
    }


    block_allsig(UNMASK_SIG);
    while(1)
    {
        sleep(10);
    }
    /*
    int          sig;
    siginfo_t    info;
    sigset_t     signal_set;
    block_allsig(UNMASK_SIG);
    sigemptyset(&signal_set);
    sigaddset(&signal_set,SIG_RETURN);
    while(1)
    {
        sig = sigwaitinfo(&signal_set,&info);
        if (sig == SIG_RETURN)
        {
            DBG("recvi signal SIG_RETURN");
        }

    }
    */

    return NULL;
}

int start_sthr()
{
    pthread_t tid;
    int i =0;

    for (i = 0; i < def_info->sthr_num; ++i)
    {   
        if(pthread_create(&tid,NULL,ths_run,NULL) < 0)  
        {   
            ERR("start write thread error！");
            return -1; 
        }   
    }   
    return 0;


}

int start_wthr()
{
    pthread_t tid;
    int i = 0;

    for (i = 0; i < def_info->wthr_num; ++i)
    {
//   DBG("w disk_num:[%d]",wthr_info[i].disk_num);
        if(pthread_create(&tid,NULL,thw_run,(void*)&wthr_info[i]) < 0) 
        { 
            ERR("start write thread error！");
            return -1;
        }
    }
    return 0;
}
