#include <sys/types.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <vector>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <pthread.h>
#include <unistd.h>

#include "baokang_handler.h"
#include "baokang_pthread.h"
//#include "vd_msg_queue.h"
//#include "log.h"
#define DEBUG printf
#define INFO printf
#define ERROR printf

using namespace std;

extern int errno;

baokang_info_s g_baokang_info[VP_BK_PIC_COUNT];

vector<connect_session> g_pic_session_list;

int baokang_create_server(int port)
{
    errno = 0;
    int lfd = socket(AF_INET, SOCK_STREAM, 0);
    if(-1 == lfd)
    {
        ERROR("lfd failed errno = %d\n", errno);
        return -1;
    }

    int option = 1;
    errno = 0;
    if(setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option)) < 0)
    {
        ERROR("lfd set resuse failed. errno = %d\n", errno);
    }

    struct sockaddr_in servaddr;
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    servaddr.sin_addr.s_addr = INADDR_ANY;

    errno = 0;
    if(-1 == bind(lfd, (sockaddr*)&servaddr, sizeof(servaddr)))
    {
        ERROR("bind error. errno = %d\n", errno);
        return -1;
    }

    errno = 0;
    if(-1 == listen(lfd, 20))
    {
        ERROR("listen error. errno = %d\n", errno);
        return -1;
    }

    return lfd;
}

void* baokang_pic_task(void* para)
{
    str_vp_msg vp_msg = {0};

    while(true)
    {
        memset(&vp_msg, 0, sizeof(str_vp_msg));

        errno = 0;
        int msg_len = sizeof(str_vp_msg) - sizeof(long);
        if(-1 == msgrcv(MSG_VP_ID, &vp_msg, msg_len, MSG_VP_BAOKANG_TYPE, 0))
        {
            ERROR("msg receive failed. errno = %d\n", errno);
            continue;
        }

        DEBUG("send pic info to baokang platform\n");
        // get the pic & send 2 baokang.
        baokang_info_s resp[VP_BK_PIC_COUNT];
        memcpy(&resp, &g_baokang_info, sizeof(baokang_info_s) * VP_BK_PIC_COUNT);

        vector<connect_session>::iterator it = g_pic_session_list.begin();
        //traversal all the connections and send pic data.
        for(; it != g_pic_session_list.end(); ++it)
        {
            //traversal all the pics.
            for(int j = 0; j < VP_BK_PIC_COUNT; j++)
            {
                if(0 == resp[j].pic_size)
                    break;

                int pack_length = sizeof(int) + resp[j].pic_size;
                DEBUG("send to fd %d %d\n", it->fd, pack_length);
                resp[j].pic_size = htonl(resp[j].pic_size);
#if 1
                for(int i = 0; i < 32; i++)
                {
                    DEBUG("%02x ", ((unsigned char*)&(resp[j])));
                }
                DEBUG("\n");
#endif
                //retry 3times if failed.
                for(int i = 0; i < 3; i++)
                {
                    errno = 0;
                    int ret = send(it->fd, &resp[j], pack_length, 0);
                    if(-1 == ret)
                    {
                        ERROR("send failed. errno = %d", errno);
                    }
                    else
                    {
                        INFO("send success.");
                        break;
                    }
                }
                // with a break before send 2nd pic.
                sleep(1);
            }
        }
    }
    return NULL;
}

void* baokang_pthread(void* arg)
{
    DEBUG("##### BAOKANG_PTHREAD start #####\n");
    pthread_t bk_pic_id; 
    if(pthread_create(&bk_pic_id, NULL, baokang_pic_task, NULL) != 0)
    {
        ERROR("create baokang_pic_task failed.\n");
        return NULL;
    }

    struct epoll_event ev, events[6];

    // size of epoll_create must lager than 0 but make no effect.
    int epfd = epoll_create(6); 
    
    int lcmd_fd   = baokang_create_server(35000);
    int lpic_fd   = baokang_create_server(45000);
    int lvideo_fd = baokang_create_server(61001);

    int cmd_fd = 0;
    int pic_fd = 0;
    int video_fd = 0;

    ev.data.fd = lcmd_fd;
    ev.events = EPOLLIN;
    errno = 0;
    if(-1 == epoll_ctl(epfd, EPOLL_CTL_ADD, lcmd_fd, &ev))
    {
        ERROR("epoll_ctl failed errno = %d\n", errno);
        return NULL;
    }

    ev.data.fd = lpic_fd;
    ev.events = EPOLLIN;
    errno = 0;
    if(-1 == epoll_ctl(epfd, EPOLL_CTL_ADD, lpic_fd, &ev))
    {
        ERROR("epoll_ctl failed errno = %d\n", errno);
        return NULL;
    }

    ev.data.fd = lvideo_fd;
    ev.events = EPOLLIN;
    errno = 0;
    if(-1 == epoll_ctl(epfd, EPOLL_CTL_ADD, lvideo_fd, &ev))
    {
        ERROR("epoll_ctl failed errno = %d\n", errno);
        return NULL;
    }

    while(true)
    {
        int fd_num = epoll_wait(epfd, events, 20, 500);
        for(int i = 0; i < fd_num; i++)
        {
            if(lcmd_fd == events[i].data.fd)
            {
                struct sockaddr_in addr;
                socklen_t len = sizeof(addr);

                errno = 0;
                cmd_fd = accept(lcmd_fd, (sockaddr*)&addr, &len);
                if(-1 == cmd_fd )
                {
                    ERROR("accept failed errno = %d\n", errno);
                    continue;
                }

                char *str = inet_ntoa(addr.sin_addr);
                INFO("accept connection from %s fd = %d\n", str, cmd_fd);

                struct timeval timeout={5,0};//5s
                setsockopt(cmd_fd, SOL_SOCKET, SO_SNDTIMEO,
                        (const void*)&timeout, sizeof(timeout));
                setsockopt(cmd_fd, SOL_SOCKET, SO_RCVTIMEO,
                        (const void*)&timeout, sizeof(timeout));

                int keepAlive = 1;    // open keepalive property. dft: 0(close)
                int keepIdle = 60;    // probe if 60s without action. dft:7200s
                int keepInterval = 5; // time interval 5s when probe. dft:75s
                int keepCount = 3;    // retry times default:9
                setsockopt(cmd_fd, SOL_SOCKET, SO_KEEPALIVE,
                        (const void*)&keepAlive, sizeof(keepAlive));
                setsockopt(cmd_fd, SOL_TCP, TCP_KEEPIDLE,
                        (const void*)&keepIdle, sizeof(keepIdle));
                setsockopt(cmd_fd, SOL_TCP, TCP_KEEPINTVL,
                        (const void*)&keepInterval, sizeof(keepInterval));
                setsockopt(cmd_fd, SOL_TCP, TCP_KEEPCNT,
                        (const void*)&keepCount, sizeof(keepCount));

                ev.data.fd = cmd_fd;
                ev.events = EPOLLIN;
                errno = 0;
                if(-1 == epoll_ctl(epfd, EPOLL_CTL_ADD, cmd_fd, &ev))
                {
                    ERROR("epoll_ctl failed errno = %d\n", errno);
                    return NULL;
                }
            }
            
            else if(lpic_fd == events[i].data.fd)
            {
                struct sockaddr_in addr;
                socklen_t len = sizeof(addr);

                errno = 0;
                pic_fd = accept(lpic_fd, (sockaddr*)&addr, &len);
                if(-1 == pic_fd )
                {
                    ERROR("accept failed errno = %d\n", errno);
                    continue;
                }

                char *str = inet_ntoa(addr.sin_addr);
                INFO("accept connection from %s\n", str);

                struct timeval timeout={5,0};//5s
                setsockopt(pic_fd, SOL_SOCKET, SO_SNDTIMEO,
                        (const void*)&timeout, sizeof(timeout));
                setsockopt(pic_fd, SOL_SOCKET, SO_RCVTIMEO,
                        (const void*)&timeout, sizeof(timeout));

                int keepAlive = 1;    // open keepalive property. dft: 0(close)
                int keepIdle = 60;    // probe if 60s without action. dft:7200s
                int keepInterval = 5; // time interval 5s when probe. dft:75s
                int keepCount = 3;    // retry times default:9
                setsockopt(pic_fd, SOL_SOCKET, SO_KEEPALIVE,
                        (const void*)&keepAlive, sizeof(keepAlive));
                setsockopt(pic_fd, SOL_TCP, TCP_KEEPIDLE,
                        (const void*)&keepIdle, sizeof(keepIdle));
                setsockopt(pic_fd, SOL_TCP, TCP_KEEPINTVL,
                        (const void*)&keepInterval, sizeof(keepInterval));
                setsockopt(pic_fd, SOL_TCP, TCP_KEEPCNT,
                        (const void*)&keepCount, sizeof(keepCount));

                ev.data.fd = pic_fd;
                ev.events = EPOLLIN;
                errno = 0;
                if(-1 == epoll_ctl(epfd, EPOLL_CTL_ADD, pic_fd, &ev))
                {
                    ERROR("epoll_ctl failed errno = %d\n", errno);
                    return NULL;
                }
                
                connect_session pic_session;
                pic_session.fd = pic_fd;
                g_pic_session_list.push_back(pic_session);
                // start pic send thread.
                //baokang_pic_handler(pic_fd);
            }

            else if(lvideo_fd == events[i].data.fd)
            {
                struct sockaddr_in addr;
                socklen_t len = sizeof(addr);

                errno = 0;
                video_fd = accept(lpic_fd, (sockaddr*)&addr, &len);
                if(-1 == video_fd )
                {
                    ERROR("accept failed errno = %d\n", errno);
                    continue;
                }

                char *str = inet_ntoa(addr.sin_addr);
                INFO("accept connection from %s\n", str);

                struct timeval timeout={5,0};//5s
                setsockopt(video_fd, SOL_SOCKET, SO_SNDTIMEO,
                        (const void*)&timeout, sizeof(timeout));
                setsockopt(video_fd, SOL_SOCKET, SO_RCVTIMEO,
                        (const void*)&timeout, sizeof(timeout));

                int keepAlive = 1;    // open keepalive property. dft: 0(close)
                int keepIdle = 60;    // probe if 60s without action. dft:7200s
                int keepInterval = 5; // time interval 5s when probe. dft:75s
                int keepCount = 3;    // retry times default:9
                setsockopt(video_fd, SOL_SOCKET, SO_KEEPALIVE,
                        (const void*)&keepAlive, sizeof(keepAlive));
                setsockopt(video_fd, SOL_TCP, TCP_KEEPIDLE,
                        (const void*)&keepIdle, sizeof(keepIdle));
                setsockopt(video_fd, SOL_TCP, TCP_KEEPINTVL,
                        (const void*)&keepInterval, sizeof(keepInterval));
                setsockopt(video_fd, SOL_TCP, TCP_KEEPCNT,
                        (const void*)&keepCount, sizeof(keepCount));

                ev.data.fd = video_fd;
                ev.events = EPOLLIN;
                errno = 0;
                if(-1 == epoll_ctl(epfd, EPOLL_CTL_ADD, video_fd, &ev))
                {
                    ERROR("epoll_ctl failed errno = %d\n", errno);
                    return NULL;
                }
            }
            
            // if connected user & received data, read data.
            else if(events[i].events & EPOLLIN)
            {
                baokang_cmd_handler(events[i].data.fd);
#if 0
                if(cmd_fd == events[i].data.fd)
                {
                    baokang_cmd_handler(cmd_fd);
                }
                else if(pic_fd == events[i].data.fd)
                {
                    baokang_pic_handler(pic_fd);
                }
                else if(video_fd == events[i].data.fd)
                {
                    baokang_video_handler(video_fd);
                }
#endif
            }
            // if connected user & has data to send, write data.
            else if(events[i].events & EPOLLOUT)
            {
                ERROR("epoll EPOLLOUT.\n");
            }
            else
            {
                ERROR("epoll failed.\n");
            }
        }
    }
}

int main()
{
    baokang_pthread(NULL);
    return 0;
}
