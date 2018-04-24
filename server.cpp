#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <netinet/in.h>
#include <errno.h>  
#include <time.h>
#include <pthread.h>
#include <limits.h>
#include <functional>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <iomanip>
#include "utility.h"
#include "udp.h"
using namespace std;

//const char *kServerIP = "127.0.0.1";
const char *kServerIP = "192.33.1.3";

int recv_cnt = 0;
int deal_cnt = 0;

char packets[kUdpPacketNum][kUdpPacketSize];
struct timespec recv_ts[kUdpPacketNum];
const long long kSecInNs = 1000000000, kUsecInNs = 1000, kMsInNs = 1000000;
const long long kSecInMs = 1000;

void Display(fstream &f, const char *label, const long long &t) {
    long long ms, us;
    ms = t/kMsInNs;
    us = (t%kMsInNs)/kUsecInNs;
	f << label << ": " << ms << "." << setw(3) << us << "\n";
}

void* DealingPacket(void *arg) {
    fstream f;
    f.open("./data.txt", ios::out);
    f.fill('0');
	struct timespec duration;
	long long ms, us;
	long long cur = 0, low = LLONG_MAX, high =0, avg = 0;
	int send_seq = 0;
    while (deal_cnt < kUdpPacketNum and send_seq < kUdpPacketNum) {
        while (deal_cnt < recv_cnt) {
            send_seq = *reinterpret_cast<int *>(packets[deal_cnt] + sizeof(struct timespec));
            struct timespec *src = (struct timespec *)packets[deal_cnt];
            struct timespec *dst = &recv_ts[deal_cnt];
            if (dst->tv_nsec < src->tv_nsec) {
                dst->tv_nsec += 1000000000;
                dst->tv_sec -= 1;
            }
            duration.tv_sec = dst->tv_sec-src->tv_sec;
            duration.tv_nsec = dst->tv_nsec-src->tv_nsec;
            ms = duration.tv_sec*kSecInMs + duration.tv_nsec/kMsInNs;
            us = (duration.tv_nsec%kMsInNs)/kUsecInNs;

            f << "send_seq:" << send_seq << " rec_seq:" << deal_cnt+1 << " duration(ms): " << ms <<
                 "." << setw(3) << us << "\n";

            cur = duration.tv_sec *kSecInNs + duration.tv_nsec ;
            low = std::min(low, cur);
            high = std::max(high, cur);
            avg += cur;
            ++deal_cnt;
        }    
    }
    Display(f, "Average Latency(ms)", avg/kUdpPacketNum);
    Display(f, "Min Latency(ms)", low);
    Display(f, "Max Latency(ms)", high);
    f.close();
    exit(0);
}

void server_entry() {
    int sockfd;
    int err, n;
    socklen_t addrlen;
    struct sockaddr_in addr_ser, addr_cli;

    SetProcessHighPriority();

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd == -1) {
        printf("socket error:%s\n", strerror(errno));
        return;
    }

    int rev_buf_size = 12 * 1024 * 1024;  // 12m buffer
    err = setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, (const char *)&rev_buf_size,sizeof(rev_buf_size));
    if (err == -1) {
        printf("setsockopt buffer size error:%s\n", strerror(errno));
        return;
    }

    bzero(&addr_ser, sizeof(addr_ser));
    addr_ser.sin_family = AF_INET;
    addr_ser.sin_addr.s_addr = ntohl(INADDR_ANY);
    addr_ser.sin_port = ntohs(kServerPort);
    err = bind(sockfd, (struct sockaddr *) &addr_ser, sizeof(addr_ser));
    if (err == -1) {
        printf("bind error:%s\n", strerror(errno));
        return;
    }
    addrlen = sizeof(struct sockaddr);
    
    pthread_t tid;
    err = pthread_create(&tid, NULL, DealingPacket, NULL);
    if (err != 0) {
        printf("Can't create dealing thread :[%s]\n", strerror(err));
        return;
    } else {
        printf("Dealing Thread created successfully\n");
    }

    while (1) {
//        printf("waiting for client......\n");
        n = recvfrom(sockfd, packets[recv_cnt], kUdpPacketSize, 0, (struct sockaddr *) &addr_cli, &addrlen);
        if (n == -1) {
            printf("recvfrom error:%s\n", strerror(errno));
            return;
        }

        clock_gettime(CLOCK_REALTIME, &recv_ts[recv_cnt]);
        ++recv_cnt;
//        printf("%d\n", recv_cnt);
    }

    exit(0);
}
