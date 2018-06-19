#ifndef __SL_WRITE_H__
#define __SL_WRITE_H__

#include "sl_com.h"
#include "sl_que.h"
#include <stdint.h>


typedef struct __wthr_info_t
{
    int thr_id;
    int cpu_id;
    int disk_num;
    int *disk_id;
}wthr_info_t;

typedef struct __node_info_t
{
    int  len;
    char buff[SIZE_M];
}node_info_t;

typedef struct __disk_info_t
{
    int             file_fd;
    int             disk_id;
    int             seg_type;
    int             w_flag;
    int64_t         time_temp;//指定间隔时间
    int64_t         file_size;//文件指定大小
    int64_t         cur_fsize;//当前文件大小
    int64_t         cur_ftime;//写入起始时间
    que_t           *fbuff;
    que_t           *bbuff;
    node_info_t     *node_info;
    char            path[PATH_MAX];
    struct    aiocb   *my_aiocb;//获取aio句柄
}disk_info_t;

wthr_info_t *wthr_info;

disk_info_t *disk_info;
int start_wthr();
int start_sthr();

#endif
