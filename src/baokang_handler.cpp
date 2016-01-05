#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <pthread.h>
#include <vector>
#include <algorithm>
#include <iostream>

//#include "log.h"
#include "baokang_handler.h"

using namespace std;

extern vector<connect_session> g_pic_session_list;

enum BAOKANG_CMD
{
    BAOKANG_CMD_GET_STATE = 0x70,
    BAOKANG_CMD_SET_TIME = 0x18,
    BAOKANG_CMD_GET_RESOLUTION = 0x31
};

typedef struct _baokang_cmd
{
    unsigned char cmd;
    unsigned char reply_flag;
    int length;
    unsigned char data[];
}baokang_cmd_s;

char* baokang_construct_response(unsigned char *data, int length);

char* baokang_get_state(baokang_cmd_s *pcmd)
{
    if(NULL == pcmd)
    {
        ERROR("bad paramater.\n");
        return NULL;
    }

    if(pcmd->cmd != BAOKANG_CMD_GET_STATE)
    {
        ERROR("bad cmd 0x%x\n", pcmd->cmd);
        return NULL;
    }

	struct tm *timenow;
    struct timeval now;

    gettimeofday(&now, NULL);
    timenow = localtime(&now.tv_sec);

    unsigned char resp[28] = {0};
    resp[0] = 0x18;
    resp[16] = 0x04;
    resp[20] = timenow->tm_year % 100;
    resp[21] = timenow->tm_mon + 1;
    resp[22] = timenow->tm_mday;
    resp[23] = timenow->tm_wday;
    resp[24] = timenow->tm_hour;
    resp[25] = timenow->tm_min;
    resp[26] = timenow->tm_sec;
    resp[27] = now.tv_usec / 1000;

    char* str = baokang_construct_response(resp, sizeof(resp));
    return str;
}

char* baokang_set_time(baokang_cmd_s *pcmd)
{
    if(NULL == pcmd)
    {
        ERROR("bad paramater.\n");
        return NULL;
    }

    if(pcmd->cmd != BAOKANG_CMD_SET_TIME)
    {
        ERROR("bad cmd 0x%x\n", pcmd->cmd);
        return NULL;
    }

    unsigned char* time_str = pcmd->data;

    for(int i = 0; i < 7; i++)
    {
        printf("0x%x ", time_str[i]);
    }
    printf("\n");
    
	struct tm timenow;
    timenow.tm_year = time_str[0] + (2000 - 1900); 
    timenow.tm_mon = time_str[1] - 1;
    timenow.tm_mday = time_str[2];
    timenow.tm_wday = time_str[3];
    timenow.tm_hour = time_str[4];
    timenow.tm_min = time_str[5];
    timenow.tm_sec = time_str[6];

    printf("%d %d %d %d %d %d %d\n", timenow.tm_year, timenow.tm_mon, 
                                     timenow.tm_mday, timenow.tm_wday,
                                     timenow.tm_hour, timenow.tm_min,
                                     timenow.tm_sec);
    time_t timep = mktime(&timenow);
    struct timeval tv;
    tv.tv_sec = timep;
    tv.tv_usec = 0;
    if(settimeofday(&tv, (struct timezone *)0) < 0)
        perror("settimeofday");

	struct tm *gettime;
    struct timeval getnow;

    gettimeofday(&getnow, NULL);
    gettime = localtime(&getnow.tv_sec);
    cout << gettime->tm_year << "-" << gettime->tm_mon << "-" 
         << gettime->tm_mday << "-" << gettime->tm_wday << " " 
         << gettime->tm_hour << ":" << gettime->tm_min << ":" 
         << gettime->tm_sec << endl;
    return NULL;
}

char* baokang_get_resolution(baokang_cmd_s *pcmd)
{
    if(NULL == pcmd)
    {
        ERROR("bad paramater.\n");
        return NULL;
    }

    if(pcmd->cmd != BAOKANG_CMD_GET_RESOLUTION)
    {
        ERROR("bad cmd 0x%x\n", pcmd->cmd);
        return NULL;
    }

#if 0
    unsigned image_width = IMAGE_WIDTH;
    unsigned image_height = IMAGE_HEIGHT;
#else
    unsigned image_width = 1920;
    unsigned image_height = 1080;
#endif 


    unsigned char resp[16] = {0};
    resp[0]  = 0x0c;

    resp[4]  = (image_width >> 24) & 0xff;
    resp[5]  = (image_width >> 16) & 0xff;
    resp[6]  = (image_width >>  8) & 0xff;
    resp[7]  = (image_width      ) & 0xff;

    resp[8]  = (image_height >> 24) & 0xff;
    resp[9]  = (image_height >> 16) & 0xff;
    resp[10] = (image_height >>  8) & 0xff;
    resp[11] = (image_height      ) & 0xff;
    
    char* str = baokang_construct_response(resp, sizeof(resp));
    return str;
}

baokang_cmd_s* baokang_cmd_parse(char* buf)
{
    int data_count = (strlen(buf) + 2) / 6;
    unsigned char* recv_data = 
        (unsigned char*)malloc(data_count * sizeof(unsigned char));
    if(NULL == recv_data)
    {
        ERROR("malloc failed\n");
        return NULL;
    }
    memset(recv_data, 0, data_count * sizeof(unsigned char));

    const char *d = " ,";
    char *p;
    char *stop;
    p = strtok(buf, d);
   
    int i = 0;
    while(p)
    {
        recv_data[i] = strtol(p, &stop, 16);
        p = strtok(NULL, d);
        i++;
    }

    baokang_cmd_s *pcmd = (baokang_cmd_s*)malloc(
            sizeof(baokang_cmd_s) + (data_count - 6) * sizeof(unsigned char));
    if(NULL == pcmd)
    {
        ERROR("malloc pcmd failed.\n");
        return NULL;
    }
    memset(pcmd, 0, 
            sizeof(baokang_cmd_s) + (data_count - 6) * sizeof(unsigned char));
    pcmd->cmd = recv_data[0];
    pcmd->reply_flag = recv_data[1];
    pcmd->length = (recv_data[5] << 24) + (recv_data[4] << 16) +
                        (recv_data[3] << 8 ) + recv_data[2];
    memcpy(pcmd->data, &(recv_data[6]),
            (data_count - 6) * sizeof(unsigned char));

    if(recv_data != NULL)
    {
        free(recv_data);
        recv_data = NULL;
    }

    return pcmd;
}

char* baokang_construct_response(unsigned char *data, int length)
{
    if((NULL == data) || (0 == length))
    {
        ERROR("bad paramater.\n");
        return NULL;
    }

    char * resp = (char*)malloc(length * 6);
    if(NULL == resp)
    {
        ERROR("malloc resp failed.\n");
        return NULL;
    }
    
    memset(resp, 0, length * 6);

    for(int i = 0; i < length; i++)
    {
        if(0 == i)
        {
            sprintf(resp, "0x%02x", data[0]);
        }
        else
        {
            sprintf(resp, "%s, 0x%02x", resp, data[i]);
        }
    }
    INFO("RESPONSE %d %s\n", strlen(resp), resp);
    return resp;
}

int baokang_cmd_handler(int fd)
{
    // read fd;
    char read_buf[512] = {0};
    int length = sizeof(read_buf);

    errno = 0;
    length = recv(fd, read_buf, length, 0);
    if(length == -1)
    {
        if(errno == EAGAIN)
        {
            ERROR("recv timeout\n");
        }
        perror("receive failed.");
        return -1;
    }
    if(length == 0)
    {
        perror("receive shutdwon.");
        close(fd);

        vector<connect_session>::iterator it = find_if(
                g_pic_session_list.begin(), g_pic_session_list.end(),
                [fd](connect_session s){return s.fd == fd;});
        if(it != g_pic_session_list.end())
        {
            g_pic_session_list.erase(it);
        }
        return 0;
    }

    INFO("receive %s\n", read_buf);
    baokang_cmd_s *pcmd = baokang_cmd_parse(read_buf);

    char* resp = NULL;
    switch(pcmd->cmd)
    {
    case BAOKANG_CMD_GET_STATE:
        DEBUG("baokang_get_state\n");
        resp = baokang_get_state(pcmd);
        break;
    case BAOKANG_CMD_SET_TIME:
        DEBUG("baokang_set_time\n");
        resp = baokang_set_time(pcmd);
        break;
    case BAOKANG_CMD_GET_RESOLUTION:
        DEBUG("baokang_get_resolution\n");
        resp = baokang_get_resolution(pcmd);
        break;
    default:
        break;
    }

    if(pcmd->reply_flag)
    {
        if(resp != NULL) 
        {
            // send the response
            send(fd, resp, strlen(resp) + 1, 0);
        }
    }

    // pcmd malloced at baokang_cmd_parse, free it here.
    if(pcmd != NULL)
    {
        free(pcmd);
        pcmd = NULL;
    }

    // resp malloced at baokang_construct_response, free it here.
    if(resp != NULL)
    {
        free(resp);
        resp = NULL;
    }
    printf("-----------------------------------------\n");
    return 0;
}

int baokang_pic_handler(int fd)
{
}

int baokang_video_handler(int fd)
{
}
