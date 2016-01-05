#ifndef __BAOKANG_HANDLER__
#define __BAOKANG_HANDLER__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _connect_session
{
    int fd;
#if 0
    pthread_t thread_id;
#endif
}connect_session;

#define DEBUG printf
#define INFO printf
#define ERROR printf
#define MSG_VP_ID 0x1234
#define MSG_VP_BAOKANG_TYPE 1
int baokang_cmd_handler(int fdf);
int baokang_pic_handler(int fdf);
int baokang_video_handler(int fdf);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // __BAOKANG_HANDLER__
