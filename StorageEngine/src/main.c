#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <aio.h>
#include "sl_conf.h"
#include "sl_read.h"
#include "sl_write.h"
#include "sl_sig.h"
#include "sl_que.h"
#include "sl_log.h"
#include "sl_ctrl.h"

//当daemon_flags
//              0。程序进入运行模式
//              1。程序进入调试模式

#define MAX_PATH 1024
#define COF_PATH "../etc/store.cfg"

int     daemon_flags = 0;
char    conf_path[MAX_PATH] = {0};
int     mct_flag = 0;

//获取运行程序的参数
int get_opt(int argc,char* argv[])
{
    int opt;

    while((opt = getopt(argc,argv,"di")) != -1)
    {
        switch(opt)
        {
            case 'd':
                daemon_flags = 1;
                break;
            case 'i':
                printf("-----param is i,[%s]\n",optarg);
                break;
            default:
                return -1;
        }
    }
    return 0;
}

//守护进程
int deamon_instance()
{
    pid_t pid;
    int i = 3;

    if((pid = fork()) < 0)  { exit(0);}
    else if (pid > 0)   { exit(0);}

    if(setsid()<0)  { exit(-1); }
    if((pid = fork()))  { exit(0);}
    if(chdir("/") < 0)  { exit(0);}
    umask(0);
    for(i = 0; i< getdtablesize(); ++i) { close(i);}


    return 0;
}

//获取当前运行程序的所在路径
int get_path()
{
    int ret = -1;
    char buff[MAX_PATH] = {0};
    char *ptr = NULL;

    ret = readlink("/proc/self/exe",buff,MAX_PATH);
    if(ret < 0) { return -1;}
    ptr = strrchr(buff,'/');
    if (ptr) { *ptr = '\0';}
    snprintf(conf_path, MAX_PATH, "%s/%s",buff,COF_PATH);
    //printf("path:[%s]\n",conf_path);

    return 0;
}

int init_wthr()
{
    int i = 0,j = 0;
    int disk_id = 0;
    wthr_info = (wthr_info_t*)malloc(sizeof(wthr_info_t) *def_info->wthr_num);
    if(wthr_info == NULL) 
    { 
        return -1;
    }

    int area = def_info->disk_num / def_info->wthr_num;
    int left = def_info->disk_num % def_info->wthr_num;
    printf("left%d\n",left);

    for(i = 0; i< def_info->wthr_num;++i)
    {
        wthr_info[i].thr_id = i;
        wthr_info[i].cpu_id = def_info->wcpu_id[i];
        if(left > 0)
        {
            left -= 1;
            wthr_info[i].disk_num = area + 1;
            printf("w disk_num:[%d]\n",wthr_info[i].disk_num);
        }
        wthr_info[i].disk_id = (int*)malloc(sizeof(int)*wthr_info[i].disk_num);
        for(j = disk_id;j < wthr_info[i].disk_num; ++j)
        {
            wthr_info[i].disk_id[j] = disk_id;
            ++disk_id;
        }
    }
    return 0;
}

int init_rthr()
{
    int i = 0;
    int disk_id = 0;
    
    rthr_info = (rthr_info_t*)malloc(sizeof(rthr_info_t) *def_info->rthr_num);
    if(rthr_info == NULL) { return -1;}

    int area = def_info->disk_num / def_info->rthr_num;
    int left = def_info->disk_num % def_info->rthr_num;

    for(i = 0; i< def_info->rthr_num;++i)
    {
        rthr_info[i].thr_id = i;
        rthr_info[i].cpu_id = def_info->rcpu_id[i];
        rthr_info[i].min_disk_id = disk_id;
        disk_id += area;
        rthr_info[i].disk_num =area;
        if(left > 0)
        {
            disk_id += 1;
            left -= 1;
            rthr_info[i].disk_num +=1;
        }
        rthr_info[i].buffer = (node_info_t**)malloc(sizeof(node_info_t*)*rthr_info[i].disk_num);
    }
    return 0;
}

int init_sthr()
{
    int i = 0;
    sthr_info = (sthr_info_t*)malloc(sizeof(sthr_info_t) *def_info->sthr_num);
    if(sthr_info == NULL) 
    { 
        return -1;
    }
    for(i = 0; i< def_info->sthr_num;++i)
    {
        sthr_info[i].thr_id = i;
        sthr_info[i].cpu_id = def_info->scpu_id[i];
    }
    return 0;
}

int init_disk()
{
    int i = 0,j = 0;
    node_info_t *ptr;

    
    disk_info = (disk_info_t*)malloc(sizeof(disk_info_t) *def_info->disk_num);
    if(disk_info == NULL) { return -1;}
    for(i = 0; i < def_info->disk_num;++i)
    {
        disk_info[i].disk_id = i;
        disk_info[i].seg_type = def_info->seg_type;
        disk_info[i].time_temp = def_info->stime;
        disk_info[i].file_size = def_info->ssize;
        disk_info[i].file_fd = -1;
        disk_info[i].w_flag = 1;//当前是否可从队列获取数据
        disk_info[i].my_aiocb = (struct aiocb*)malloc(sizeof(struct aiocb));
        strcpy(disk_info[i].path,def_info->path[i]);
        ring_init(disk_info[i].fbuff, node_info_t,def_info->node_num);
        ring_init(disk_info[i].bbuff, node_info_t,def_info->node_num);
        if((ptr = (node_info_t*)malloc(sizeof(node_info_t)*def_info->node_num))==NULL) { return -1;}
        for(j = 0;j < def_info->node_num;++j) 
        { 
            if(!write_inval(disk_info[i].fbuff,node_info_t,&ptr[j])) 
            {  
                return -1;
            } 
        }
    } 
    return 0;
}

int init_data(char* conf)
{
    if (open_conf(conf) < 0) { return -1;}
    def_info = (def_info_t*)malloc(sizeof(def_info_t));
    if(def_info == NULL)    { return -1;}
    if (get_val_single("base.disk_num",&def_info->disk_num,TYPE_INT) < 0) { return -1;}
    if (get_val_single("base.rthr_num",&def_info->rthr_num,TYPE_INT) < 0) { return -1;}
    if (get_val_single("base.wthr_num",&def_info->wthr_num,TYPE_INT) < 0) { return -1;}
    if (get_val_single("base.sthr_num",&def_info->sthr_num,TYPE_INT) < 0) { return -1;}


    def_info->rcpu_id = (int*)malloc(sizeof(int)*def_info->rthr_num);
    if(get_val_arry("base.rcpu_id",(void**)&def_info->rcpu_id,def_info->rthr_num,TYPE_INT)<0) { return -1;}

    def_info->wcpu_id = (int*)malloc(sizeof(int)*def_info->wthr_num);
    if(get_val_arry("base.wcpu_id",(void**)&def_info->wcpu_id,def_info->wthr_num,TYPE_INT)<0) { return -1;}

    def_info->scpu_id = (int*)malloc(sizeof(int)*def_info->sthr_num);
    if(get_val_arry("base.scpu_id",(void**)&def_info->scpu_id,def_info->sthr_num,TYPE_INT)<0) { return -1;}

    def_info->path = (char **)malloc(sizeof(char*) *def_info->disk_num);
    if(get_val_arry("base.disk_path",(void**)def_info->path,def_info->disk_num,TYPE_STRING)<0)  { return -1;}


    if (get_val_single("base.seg_type",&def_info->seg_type,TYPE_INT) < 0) { return -1;}

    if (get_val_single("base.stime",&def_info->stime,TYPE_LONG) < 0) { return -1;}

    if (get_val_single("base.ssize",&def_info->ssize,TYPE_LONG) < 0) { return -1;}
    def_info->ssize *= SIZE_M;

    if (get_val_single("base.node_num",&def_info->node_num,TYPE_INT) < 0) { return -1;}

    if (get_val_single("base.node_size",&def_info->node_size,TYPE_LONG) < 0) { return -1;}
    def_info->node_size *= SIZE_M;

    if (get_val_single("base.conf_path.ctrl_file",&def_info->ctrl_file,TYPE_STRING) < 0) { return -1;}

    if (get_val_single("base.conf_path.log_file",&def_info->log_file,TYPE_STRING) < 0) { return -1;}


    if (init_disk() < 0) { return -1;}

    if (init_wthr() < 0) { return -1;}

    if (init_rthr() < 0) { return -1;}

    if (init_sthr() < 0) { return -1;}

    return 0;
}

int print_data()
{
    int i = 0;
    printf("disk_num:[%d]\n",def_info->disk_num);
    for(i = 0;i < def_info->disk_num; ++i)
        printf("\tdisk_path:[%s]\n",def_info->path[i]);

    printf("rthr_num:[%d]\n",def_info->rthr_num);
    for(i = 0;i < def_info->rthr_num; ++i)
        printf("\trcpu_id:[%d]\n",def_info->rcpu_id[i]);

    printf("wthr_num:[%d]\n",def_info->wthr_num);
    for(i = 0;i < def_info->wthr_num; ++i)
        printf("\twcpu_id:[%d]\n",def_info->wcpu_id[i]);

    printf("sthr_num:[%d]\n",def_info->sthr_num);
    for(i = 0;i < def_info->sthr_num; ++i)
        printf("\tscpu_id:[%d]\n",def_info->scpu_id[i]);

    printf("seg_type:[%d]\n",def_info->seg_type);
    printf("node_num:[%d]\n",def_info->node_num);
    printf("node_size:[%ld]\n",def_info->node_size);
    printf("stime:[%ld]\n",def_info->stime);
    printf("ssize:[%ld]\n",def_info->ssize);
    printf("ctrl_file:[%s]\n",def_info->ctrl_file);
    printf("log_file:[%s]\n",def_info->log_file);

    return 0;
}

void func(int argc,char *argv[])
{
    int flag = atoi(argv[1]);

    printf("recv cmd:[%s]\n",argv[0]);

    if(flag == 0) { mct_flag = 0;}
    else { mct_flag = 1;}

}

int start_ctrl()
{
    if (ctrl_init(def_info->ctrl_file) < 0) { return -1;}

    ctrlor_add("stop-main", "stop main pthread while 0-stop 1-start",func);

    return 0;

}

int main(int argc,char *argv[])
{
    char log_type[8] = {0};

    if(get_opt(argc,argv) < 0)
    {
        printf("parse option error\n");
        return -1;
    }
    if (!daemon_flags)  
    { 
        deamon_instance();
        strcpy(log_type,"f_cat");
    }
    else 
    { 
        strcpy(log_type,"o_cat"); 
    }

    if (get_path() < 0) { return -1;}
    if (init_data(conf_path) < 0) { return -1;}

    printf("-----------------\n");
    print_data();

    if (open_log(def_info->log_file,log_type)) { return -1;}

    if (block_allsig(MASK_SIG) < 0) { return -1;}

    if (start_ctrl() < 0) { return -1;}

    if (start_sthr() < 0) { return -1;}

    if (start_wthr() < 0) { return -1;}

    if (start_rthr() < 0) { return -1;}

    while(1)
    {
        while(mct_flag == 1)
        {
            sleep(1);
        }
        DBG("qing meng");
        sleep(1);
    }
    return 0;
}
