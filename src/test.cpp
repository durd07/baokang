#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h> 

#include <iostream>
using namespace std;

int main()
{
    struct sockaddr_in addr;

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(35000);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    connect(fd, (struct sockaddr*)&addr, sizeof(struct sockaddr));

    while(1)
    {
        cout << "1 get status" << endl;
        cout << "2 set time" << endl;
        cout << "3 get resolution" << endl;

        int a;
        cin >> a;
        switch(a)
        {
            case 1:
                {
                char cmd1[] = "0x70, 0x01, 0x00, 0x00, 0x00, 0x00";
                send(fd, cmd1, 256, 0);

                char buf[512] = {0};
                recv(fd, buf, 512, 0);
                cout << buf << endl;
                break;
                }
            case 2:
                {
                char cmd2[] = "0x18, 0x00, 0x08, 0x00, 0x00, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77";
                send(fd, cmd2, 256, 0);
                break;
                }
            case 3:
                {
                char cmd3[] = "0x31, 0x01, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03";
                send(fd, cmd3, 256, 0);
                char buf1[512] = {0};
                recv(fd, buf1, 512, 0);
                cout << buf1 << endl;
                break;
                }
        }
    }
}
