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
#include "utility.h"
#include "udp.h"

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
		printf("duration:%lus %lums\n", time_end_.tv_sec-time_start_.tv_sec,
			   (time_end_.tv_nsec-time_start_.tv_nsec)/1000000);
	}

private:
	struct timespec time_start_, time_end_;
};

void client_entry() {
    int sockfd;
    socklen_t addrlen;
    int n;
    struct sockaddr_in addr_ser, addr_ser2;
    char sendline[kUdpPacketSize];

    SetProcessHighPriority();

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd == -1) {
        printf("socket error:%s\n", strerror(errno));
        return;
    }

    int sent_buf_size = 12 * 1024 * 1024;  // 12m buffer
    int err = setsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, (const char *)&sent_buf_size,sizeof(sent_buf_size));
    if (err == -1) {
        printf("setsockopt buffer size error:%s\n", strerror(errno));
        return;
    }

    bzero(&addr_ser, sizeof(addr_ser));
    addr_ser.sin_family = AF_INET;
    addr_ser.sin_addr.s_addr = inet_addr(kServerIP);
    addr_ser.sin_port = htons(kServerPort);

    addr_ser2 = addr_ser;
    addr_ser2.sin_port = htons(kServerPort-1);

    addrlen = sizeof(addr_ser);

//    Display();
    int counter = kUdpPacketNum;
    TimeStamp ts;
    while (counter--) {
        *reinterpret_cast<int *>(sendline + sizeof(struct timespec)) = kUdpPacketNum - counter;
        int txqPre, txqPost;
        ioctl(sockfd, TIOCOUTQ, &txqPre);
        clock_gettime(CLOCK_REALTIME, (struct timespec*)sendline);
        n = sendto(sockfd, sendline, kUdpPacketSize, MSG_DONTWAIT, (struct sockaddr *) &addr_ser, addrlen);
        n = sendto(sockfd, sendline, kUdpPacketSize, MSG_DONTWAIT, (struct sockaddr *) &addr_ser2, addrlen);
        if (n == -1) {
            printf("sendto error:%s\n", strerror(errno));
            return;
        }
        ioctl(sockfd, TIOCOUTQ, &txqPost);
//        printf("seq %d udp buffer add %d\n", kUdpPacketNum-counter,txqPost - txqPre);
        usleep(kInterval);
//        printf("Continue: ");
//        scanf("%s", sendline);
    }
    exit(0);
}
