/*
 * owtraffic.cpp
 *
 *  Created on: Apr 3, 2018
 *      Author: changqwa
 */
#include <sched.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <getopt.h>
#include <iostream>
#include <string>
#include "owtraffic.h"
#include "NetworkNamespace.h"
#include "udp.h"

std::string kClientNs = "cpe";
std::string kServerNs = "cbr8";

std::string kClientIP = "105.29.1.88";
int kClientPort = 0;

std::string kServerIP = "192.33.1.3";
int kServerPort = 44444;

std::string kOuputFile = "./data.txt";
int kInterval = 800; // unit is usec.

int kUdpPacketNum = 10000;
const int kHeaderSize = 20 + 8; // ip(20)+udp(8)
const int kMinUdpPayloadSize = 32; // time + send id
int kUdpPayloadSize = 222; // plus kHeaderSize. totally equal to 250

void fork_server_client() {
    // child process because return value zero
    if (fork()==0) {
        printf("Hello from Server pid %d!\n", getpid());
        if (!set_netns(kServerNs.c_str())) exit(-1);
        if (!deescalate()) exit(-1);
        server_entry();
    }

    // 10ms guarantee the server is launched before client
    usleep(10000);

    if (fork()==0) {
        printf("Hello from Client pid %d!\n", getpid());
        if (!set_netns(kClientNs.c_str())) exit(-1);
        if (!deescalate()) exit(-1);
        client_entry();
    }
}

void PrintHelp() {
    std::cout <<
            "--client-ns <ns>:              Client network namespace\n"
            "--client-ip <x.x.x.x>:         Client Ip address\n"
            "--client-port <n>:             Client Port\n"
            "--server-ns <ns>:              Server network namespace\n"
            "--server-ip <x.x.x.x>:         Server Ip address\n"
            "--server-port <n>:             Server Port\n"
            "--output-file <fname>:         File to write to\n"
            "--interval <val>               Unit is usec\n"
            "--packet-num <val>             Packet number\n"
            "--packet-size <val>            Using udp, at least " << kHeaderSize + kMinUdpPayloadSize << "\n"
            "--help:                        Show help\n";
    exit(1);
}

void ProcessArgs(int argc, char** argv) {
    enum {
        ClientNs = 1001,
        ClientIp,
        ClientPort,
        ServerNs,
        ServerIp,
        ServerPort,
        OuputFile,
        Interval,
        PacketNum,
        PacketSize,
        Help
    };
    const option long_opts[] = {
        {"client-ns", 1, nullptr, ClientNs},
        {"client-ip", 1, nullptr, ClientIp},
        {"client-port", 1, nullptr, ClientPort},
        {"server-ns", 1, nullptr, ServerNs},
        {"server-ip", 1, nullptr, ServerIp},
        {"server-port", 1, nullptr, ServerPort},
        {"output-file", 1, nullptr, OuputFile},
        {"interval", 1, nullptr, Interval},
        {"packet-num", 1, nullptr, PacketNum},
        {"packet-size", 1, nullptr, PacketSize},
        {"help", 0, nullptr, Help},
        {nullptr, 0, nullptr, 0}
    };

    while (true) {
        const auto opt = getopt_long(argc, argv, "", long_opts, 0);
        if (-1 == opt) break;

        switch (opt) {
            case ClientNs:
                kClientNs = std::string(optarg);
                break;
            case ClientIp:
                kClientIP = std::string(optarg);
                break;
            case ClientPort:
                kClientPort = std::stoi(optarg);
                break;
            case ServerNs:
                kServerNs = std::string(optarg);
                break;
            case ServerIp:
                kServerIP = std::string(optarg);
                break;
            case ServerPort:
                kServerPort = std::stoi(optarg);
                break;
            case OuputFile:
                kOuputFile = std::string(optarg);
                break;
            case Interval:
                kInterval = std::stoi(optarg);
                break;
            case PacketNum:
                kUdpPacketNum = std::stoi(optarg);
                break;
            case PacketSize:
            {
                int pakcet_size = std::stoi(optarg);
                if (pakcet_size < kHeaderSize + kMinUdpPayloadSize) {
                    PrintHelp();
                } else {
                    kUdpPayloadSize = pakcet_size - kHeaderSize;
                }
                break;
            }
            default:
                PrintHelp();
        }
    }
}

int main(int argc, char** argv) {
    ProcessArgs(argc, argv);
    if (already_in_namespace()) return -1;
    pid_t pid;
    fork_server_client();
    while ((pid = waitpid(-1, NULL, 0) != 0)) {
       printf("Exit from Pid %d!\n", pid);
       if (errno == ECHILD) {
          break;
       }
    }
    return 0;
}
