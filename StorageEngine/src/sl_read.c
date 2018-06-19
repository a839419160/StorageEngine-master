#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include "sl_read.h"
#include "sl_log.h"

//获取数据
//对数据进行处理
//选择一个归自己管理的空闲磁盘队列
//获取磁盘队列一个空闲节点
//向节点中写数据
//将队列节点，放入数据队列

char *get_pdata()
{
    static char *tmp = "qing meng 201314";
    return tmp;
}

int get_wdisk(rthr_info_t *info)
{
    static uint64_t i = 0;
    ++i;

    return (info->min_disk_id + (i % info->disk_num));
}

void *thr_run(void* args)
{
    int i = 0;
    int disk_id = 0;
    int disk_rid = 0;
    char *ptr = NULL;
    rthr_info_t *r_info = (rthr_info_t*)args;

    for( i = 0; i < r_info->disk_num; ++i)
    {
        //从每一个磁盘空闲队列中获取一个节点，将地址赋值给处理线程的buffer，通过buffer来操作空间
        while(!read_outval(disk_info[i + r_info->min_disk_id].fbuff,node_info_t,r_info->buffer[i]))
        {
            ERR("get %d disk idle que failed",disk_id);
            usleep(10);
            continue;
        }
        r_info->buffer[i]->len = 0;
    }
    while(1)
    {
        //获取数据
        ptr = get_pdata();
        //获取一个即将写入数据的磁盘ID
        disk_id = get_wdisk(r_info);
        //获取已经获取的磁盘的节点下标
        disk_rid = disk_id - r_info->min_disk_id;

        if ((int64_t)(r_info->buffer[disk_rid]->len + strlen(ptr)) > (int64_t)SIZE_M)
        {
            //如果磁盘的节点数据已经即将写满1M，那就把节点放入数据队列bbuff中供线程获取
            while(!write_inval(disk_info[disk_id].bbuff,node_info_t,r_info->buffer[disk_rid]))
            {
                usleep(10);
                continue;
            }
            //将节点放入数据队列中后，重新获取一个空闲节点准备写数据，并且将节点的写入长度重置为0
            while(!read_outval(disk_info[disk_id].fbuff,node_info_t,r_info->buffer[disk_rid]))
            {
              //  ERR("get %d disk idle que failed",disk_id);
                usleep(10);
                continue;
            }
            r_info->buffer[disk_rid]->len = 0;
        }
        //将数据拷贝到节点中
        memcpy(r_info->buffer[disk_rid]->buff + r_info->buffer[disk_rid]->len, ptr, strlen(ptr));
        //对节点中数据的长度加上这次写入的数据长度
        r_info->buffer[disk_rid]->len += strlen(ptr);
    }
    return NULL;
}

int start_rthr()
{
    int i = 0;
    pthread_t tid;

    for (i = 0;i < def_info->rthr_num; ++i)
    {
       if(pthread_create(&tid,NULL,thr_run,(void*)&rthr_info[i]) < 0) 
       { 
           ERR("start read pthread error!\n");
           return -1;
       }
    }

    return 0;
}
