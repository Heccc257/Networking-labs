#include <time.h>
#include <iostream>
#include <unistd.h>//close
#include <arpa/inet.h>//网络字节序和主机字节序库转换
#include <stdlib.h>//atoi
#include <cstring>

#include <netinet/in.h>
#include "rtp.h"
#include "datatrans.h"
#include "util.h"

bool sendRTP(int src, rtp_packet_t *pack, sockaddr_in dest) {

    
    socklen_t addrlen = sizeof(dest);
    // std::cerr <<"in addrLen = " << addrlen << '\n';
    ssize_t ret = sendto(src, (const void*)pack, pack->rtp.length + HeaderSize, 0, (struct sockaddr*)&dest, addrlen);
    if( ret < HeaderSize) return 0;
    return 1;
}


const int BUFFSIZE = 2048;
// char rcvbuf[BUFFSIZE];
/*
// 接收报文, 返回nullptr代表接收失败
// 自动丢弃损坏的报文
// 可能会一次性读取不止一个包的字节，但是只保留第一个
*/
rtp_packet_t* revRTP(int my, sockaddr_in *fr, char *rcvbuf) {
    // clock_t start = clock();
    rtp_packet_t *rev = new rtp_packet_t();
    socklen_t frlen = sizeof(*fr);

    // std::cerr << "start to receive HD\n";
    ssize_t ret = recvfrom(my, (void *)rcvbuf, BUFFSIZE, 0, (struct sockaddr*)fr, &frlen);
    if(ret < HeaderSize) return nullptr;
    memcpy((char*)&rev->rtp, rcvbuf, HeaderSize);
    memcpy((char*)rev->payload, rcvbuf+HeaderSize, rev->rtp.length);
    // std::cerr << "ret = " << ret << " rev " << rev->rtp.length <<" " << (int)rev->rtp.type << "\n";
    // std::cerr << "rcvRTP  fr : " << "len = " << frlen << " dest = " << inet_ntoa(fr->sin_addr) << " " << ntohs( fr->sin_port ) << '\n';

    rev->payload[rev->rtp.length] = 0;
    // std::cerr << "receive ret = " << ret <<" type = " << (int)rev->rtp.type << " len = " <<rev->rtp.length << " seq = " << rev->rtp.seq_num <<" damaged: " << rev->Damaged() << " checksum = " << rev->rtp.checksum << " len = " << rev->rtp.length <<" " << compute_checksum((const void*)rev, rev->rtp.length + HeaderSize) << "\n";

    // std::cerr << "type = " << rev->rtp.type <<" seq = " << rev->rtp.seq_num << " len = " << rev->rtp.length << "\n";
    // std::cerr<<"checksum = " << rev->rtp.checksum << "\n";
    // std::cerr<<"payload = " << rev->payload << "\n";
    // std::cerr<<"damaged "  << rev->Damaged() << "\n";
    // if(rev->Damaged()) std::cerr<<"data> packet damaged\n";
    // CheckSum fail
    if(rev->Damaged()) {
        // std::cerr<<"data> packet damaged\n";
        return nullptr;
    }
    
    return rev;
}
