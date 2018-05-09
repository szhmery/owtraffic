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
#include <vector>
#include "SchedPriority.h"
#include "udp.h"
using namespace std;

#define PrintServer(fmt,args...) printf("Server " fmt, ##args)

struct PacketInfo {
    struct timespec send_ts; // sent by client
    unsigned int send_seq;   // sent by client
    struct timespec recv_ts; // stamped by server main thread
};

struct PacketBuffer {
    PacketBuffer() : recv_buf(NULL), deal_buf(NULL), deal_done(true) {}
    vector<PacketInfo> *recv_buf;
    vector<PacketInfo> *deal_buf;
    bool deal_done; //only this is false, dealing thread will start to work.

    void Swap() {
        if (not deal_done) {
            PrintServer("FATAL error. Dealing thread does not"
                        " have enough time to dealing packets\n");
            exit(1);
        }

        swap(recv_buf, deal_buf);
        deal_done = false;
//        PrintServer("deal_done false %ld\n", time(0));
    }

    void SetPacketNumber() {
        const int kMaxMTUSize = 2000;
        buffer1 = vector<PacketInfo>(kUdpPacketNum + (kMaxMTUSize/sizeof(PacketInfo)+1));
        buffer2 = buffer1;
        recv_buf = &buffer1;
        deal_buf = &buffer2;
    }

private:
    vector<PacketInfo> buffer1;
    vector<PacketInfo> buffer2;
};

PacketBuffer packet_buf;

struct PacketDealer {
    static const long long kSecInNs = 1000000000;
    static const long long kSecInMs = 1000;
    static const long long kUsecInNs = 1000;
    static const long long kMsInNs = 1000000;

    static void Display(fstream &f, const char *label, const long long &t) {
        long long ms, us;
        ms = t/kMsInNs;
        us = (t%kMsInNs)/kUsecInNs;
//        f << label << ": " << ms << "." << setw(3) << us << "\n";
        f << label << " " << ms << setw(3) << us << "\n";
    }

    static void DealPacketBuffer(string temp_file, vector<PacketInfo> &packets) {
        fstream f;
        f.open(temp_file.c_str(), ios::out);
        f.fill('0');
        struct timespec duration;
        long long ms, us;
        long long cur = 0, low = LLONG_MAX, high =0, avg = 0;
        int deal_cnt = 0;
        while (deal_cnt < kUdpPacketNum) {
//            unsigned int send_seq = packets[deal_cnt].send_seq;
            struct timespec *src = &packets[deal_cnt].send_ts;
            struct timespec *dst = &packets[deal_cnt].recv_ts;
            if (dst->tv_nsec < src->tv_nsec) {
                dst->tv_nsec += 1000000000;
                dst->tv_sec -= 1;
            }
            duration.tv_sec = dst->tv_sec-src->tv_sec;
            duration.tv_nsec = dst->tv_nsec-src->tv_nsec;
            ms = duration.tv_sec*kSecInMs + duration.tv_nsec/kMsInNs;
            us = (duration.tv_nsec%kMsInNs)/kUsecInNs;

//            f << "send_seq:" << send_seq << " rec_seq:" << deal_cnt+1 << " duration(ms): " << ms <<
//                 "." << setw(3) << us << "\n";
            f << deal_cnt+1 << " " << ms << setw(3) << us << "\n";

            cur = duration.tv_sec *kSecInNs + duration.tv_nsec ;
            low = std::min(low, cur);
            high = std::max(high, cur);
            avg += cur;
            ++deal_cnt;
        }
//        Display(f, "Average Latency(ms)", avg/kUdpPacketNum);
//        Display(f, "Min Latency(ms)", low);
//        Display(f, "Max Latency(ms)", high);

        Display(f, "Average ", avg/kUdpPacketNum);
        Display(f, "Min ", low);
        Display(f, "Max ", high);
        f << "END";

        f.close();
    }

    static void* DealingPacketThread(void *arg) {
        fstream f;
        f.open(kOuputFile.c_str(), ios::out);
        f.close();

        string temp_file = kOuputFile  + "_temp";
        const int k100Ms = 100000;
        while (1) {
            while (packet_buf.deal_done) usleep(k100Ms);
//            PrintServer("DealPacketBuffer before %ld\n", time(0));
            vector<PacketInfo> &packets = *packet_buf.deal_buf;
            DealPacketBuffer(temp_file, packets);
            packet_buf.deal_done = true;
            if (unlink(kOuputFile.c_str()) != 0) {
                PrintServer("Fail to remove output file %s\n", kOuputFile.c_str());
                exit(1);
            }
            if (rename(temp_file.c_str(), kOuputFile.c_str()) != 0) {
                PrintServer("Fail to rename temp file %s to output file %s\n",
                            temp_file.c_str(), kOuputFile.c_str());
                exit(1);
            }
//            PrintServer("DealPacketBuffer after %ld\n", time(0));
        }
    }
};

void server_entry() {
    int sockfd;
    int err, n;
    socklen_t addrlen;
    struct sockaddr_in addr_ser, addr_cli;

    SetProcessHighPriority();
    packet_buf.SetPacketNumber();

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd == -1) {
        PrintServer("socket error:%s\n", strerror(errno));
        return;
    }

    int rev_buf_size = 12 * 1024 * 1024;  // 12m buffer
    err = setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, (const char *)&rev_buf_size,sizeof(rev_buf_size));
    if (err == -1) {
        PrintServer("setsockopt buffer size error:%s\n", strerror(errno));
        return;
    }

    bzero(&addr_ser, sizeof(addr_ser));
    addr_ser.sin_family = AF_INET;
    addr_ser.sin_addr.s_addr = ntohl(INADDR_ANY);
    addr_ser.sin_port = ntohs(kServerPort);
    err = bind(sockfd, (struct sockaddr *) &addr_ser, sizeof(addr_ser));
    if (err == -1) {
        PrintServer("bind error:%s\n", strerror(errno));
        return;
    }
    addrlen = sizeof(struct sockaddr);
    
    pthread_t tid;
    err = pthread_create(&tid, NULL, PacketDealer::DealingPacketThread, NULL);
    if (err != 0) {
        PrintServer("Can't create dealing thread :[%s]\n", strerror(err));
        return;
    } else {
        PrintServer("Dealing Thread created successfully\n");
    }

    while (1) {
//        PrintServer("waiting for client......\n");
        int recv_cnt = 0;
        vector<PacketInfo> &packets = *packet_buf.recv_buf;
        while (recv_cnt < kUdpPacketNum) {
            n = recvfrom(sockfd, &packets[recv_cnt], kUdpPayloadSize, 0, (struct sockaddr *) &addr_cli, &addrlen);
            if (n == -1) {
                PrintServer("recvfrom error:%s\n", strerror(errno));
                return;
            }

            clock_gettime(CLOCK_REALTIME, &packets[recv_cnt].recv_ts);
            ++recv_cnt;
//        PrintServer("%d\n", recv_cnt);
        }
        packet_buf.Swap();
    }

    exit(0);
}
