#ifndef _BAOKANG_PTHREAD_
#define _BAOKANG_PTHREAD_

#ifdef __cplusplus
extern "C" {
#endif

#define VP_PICTURE_BASESIZE    (1024*400*4)
#define VP_BK_PIC_COUNT       (4)

typedef struct _baokang_info
{
    int  pic_size;
    char pic[VP_PICTURE_BASESIZE];
}baokang_info_s;

typedef struct 
{
    long type;
    char data[256];
    int  data_size;
}str_vpuniview;
typedef struct 
{
    long type;
    char data[256];
    int  data_size;
}str_baokang;
typedef struct
{
	long type;
	str_vpuniview uniview;
    str_baokang baokang;
}str_vp_msg;

extern baokang_info_s g_baokang_info[VP_BK_PIC_COUNT];

void* baokang_pthread(void* arg);

#ifdef __cplusplus
}
#endif

#endif
