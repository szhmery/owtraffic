#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <netinet/in.h>
#include <time.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include "SchedPriority.h"
#include "udp.h"

#define PrintClient( fmt,args...) printf("Client " fmt, ##args)

struct TimeStamp {
	TimeStamp() {
	   clock_gettime(CLOCK_REALTIME, &time_start_);
	}

	~TimeStamp() {
		clock_gettime(CLOCK_REALTIME, &time_end_);
		if (time_end_.tv_nsec < time_start_.tv_nsec) {
			time_end_.tv_nsec += 1000000000;
			time_end_.tv_sec -= 1;
		}
		PrintClient("duration:%lus %lums\n", time_end_.tv_sec-time_start_.tv_sec,
			   (time_end_.tv_nsec-time_start_.tv_nsec)/1000000);
	}

private:
	struct timespec time_start_, time_end_;
};

void PrintClientPort(int sockfd) {
    struct sockaddr_in sin;
    socklen_t len = sizeof(sin);
    if (getsockname(sockfd, (struct sockaddr *)&sin, &len) == -1)
        PrintClient("getsockname");
    else
        PrintClient("port number %d\n", ntohs(sin.sin_port));
}

void client_entry() {
    int sockfd;
    socklen_t addrlen;
    int n;
    struct sockaddr_in addr_ser;
    char sendline[kUdpPayloadSize];

    SetProcessHighPriority();

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd == -1) {
        PrintClient("socket error:%s\n", strerror(errno));
        return;
    }

    struct sockaddr_in addr_cli;

    bzero(&addr_cli, sizeof(addr_cli));
    addr_cli.sin_family = AF_INET;
    addr_cli.sin_addr.s_addr = inet_addr(kClientIP.c_str());
    addr_cli.sin_port = ntohs(kClientPort); // If 0, any local port will do.
    int err = bind(sockfd, (struct sockaddr *) &addr_cli, sizeof(addr_cli));
    if (err == -1) {
        PrintClient("bind ip %s error:%s\n", kClientIP.c_str(), strerror(errno));
        return;
    }

    if (kClientPort == 0) {
        PrintClientPort(sockfd);
    }

    int sent_buf_size = 12 * 1024 * 1024;  // 12m buffer
    err = setsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, (const char *)&sent_buf_size,sizeof(sent_buf_size));
    if (err == -1) {
        PrintClient("setsockopt buffer size error:%s\n", strerror(errno));
        return;
    }

    bzero(&addr_ser, sizeof(addr_ser));
    addr_ser.sin_family = AF_INET;
    addr_ser.sin_addr.s_addr = inet_addr(kServerIP.c_str());
    addr_ser.sin_port = htons(kServerPort);

    addrlen = sizeof(addr_ser);

//    Display();
    unsigned int cnt = 0;
    TimeStamp ts;
    while (++cnt) { // transmit at most 2^32 packets
        *reinterpret_cast<unsigned int *>(sendline + sizeof(struct timespec)) = cnt;
        int txqPre, txqPost;
        ioctl(sockfd, TIOCOUTQ, &txqPre);
        clock_gettime(CLOCK_REALTIME, (struct timespec*)sendline);
        n = sendto(sockfd, sendline, kUdpPayloadSize, MSG_DONTWAIT, (struct sockaddr *) &addr_ser, addrlen);
        if (n == -1) {
            PrintClient("sendto error:%s\n", strerror(errno));
            return;
        }
        ioctl(sockfd, TIOCOUTQ, &txqPost);
//        PrintClient("seq %d udp buffer add %d\n", kUdpPacketNum-counter,txqPost - txqPre);
        usleep(kInterval);
//        PrintClient("Continue: ");
//        scanf("%s", sendline);
    }
    exit(0);
}
