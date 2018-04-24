#ifndef __UDP__H__ 
#define __UDP__H__

//#include <sched.h>

// common
const int kServerPort = 44444;
extern const char *kServerIP;

const int kUdpPacketNum = 10000;
const int kUdpPacketSize = 222; // plus ip(20)+udp(8).totally equal to 250

const int kInterval = 8000; // unit is usec.

#endif /* __UDP__H__ */
