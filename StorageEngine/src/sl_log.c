#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include "sl_log.h"


zlog_category_t *log_handle = NULL;

int open_log(char *path, char *mode)
{
    if (path == NULL || mode == NULL) { return -1;}

    if (zlog_init(path)) 
    { 
        printf("log system init failed!\n");
        return -1;
    }
    if ((log_handle = zlog_get_category(mode)) == NULL) 
    { 
        printf("init log handle error!\n");
        return -1; 
    }

    return 0;
}

void close_log()
{
    zlog_fini();
}
