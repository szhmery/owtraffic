#ifndef __UDP__H__ 
#define __UDP__H__

#include <string>

extern std::string kClientIP;
extern int kClientPort;

extern std::string kServerIP;
extern int kServerPort;

extern std::string kOuputFile;

extern int kInterval; // unit is usec.
extern int kUdpPacketNum;
extern int kUdpPayloadSize; // plus ip(20)+udp(8)

#endif /* __UDP__H__ */
