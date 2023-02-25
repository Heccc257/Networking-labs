#include <unistd.h>//close
#include <arpa/inet.h>//网络字节序和主机字节序库转换
#include <stdlib.h>//atoi
#include <netinet/in.h>
#include <string.h>
#include <time.h>
#include <vector>
#include <iostream>
#include <cstdio>
#include <sys/time.h>
#include "rtp.h"
#include "receiver_def.h"
#include "datatrans.h"
#include "util.h"

namespace recvVar {
    uint32_t window_size;
    int sock;
    struct sockaddr_in server;
    struct sockaddr_in client;
    const int BUFFSIZE = 2048;
    char rcvbuf[BUFFSIZE];
}

int initReceiver(uint16_t port, uint32_t window_size) {
    // std::cerr << "recv begin "<< " port = "<<port << "\n";
    using recvVar::sock;
    using recvVar::server;
    using recvVar::client;

    recvVar::window_size = window_size;

    // 绑定任意一个ip的socket
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if(sock < 0) return -1;


    // std::cerr << "socket OK " << sock << "\n"; 

    memset(&server, 0, sizeof(server));
    memset(&client, 0, sizeof(client));
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    if( bind(sock, (struct sockaddr*)&server, sizeof(server)) < 0 ) {
        std::cerr<<"recv> socket 绑定失败!!!\n";
        close(sock);
        return -1;
    }
    std::cerr << "recv> Bind OK\n";
    //等待客户端的UDP连接和Start报文  
    socklen_t addr_len = sizeof(client);
    clock_t timer = clock();
    rtp_packet_t* rtpStart = nullptr;
    

    // 设置超时
    // 2s未收到视为超时
    struct timeval timeout;
    timeout.tv_sec = 2;
    timeout.tv_usec = 0;
    if( setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0 ) {
        std::cerr << "recv> setsocketopt failed\n";
    } else {
        std::cerr << "recv> setsocketopt sucess\n";
    }
    
    rtpStart = revRTP(sock, &client, recvVar::rcvbuf);
    std::cerr <<"recv> rtpStart received null? " << (rtpStart == nullptr) << "\n";

    if(rtpStart == nullptr || rtpStart->rtp.type != RTP_START) {
        std::cerr<<"recv> Start error\n";
        close(sock);
        return -1;
    }
    // ACK
    rtp_packet_t* ACK = new rtp_packet_t(RTP_ACK, rtpStart->rtp.seq_num, nullptr);
    std::cerr<<"recv> begin send ACK\n";
    // sleep 50ms
    // usleep(1000000);
    sendRTP(sock, ACK, client);
    std::cerr<<"recv> connect success\n";
    return 0;
}

void Download(rtp_packet_t** packs, size_t idx, FILE* fp) {
    rtp_packet_t* pack = packs[idx];
    packs[idx] = nullptr;
    size_t ret =  fwrite(pack->payload, 1, pack->rtp.length, fp);
    // std::cerr <<"recv> Download ret = " << ret << '\n';
}

int recvMessage(char* filename) {
    std::cerr << "recv> Begin rcvMessage\n";
    using recvVar::sock;
    using recvVar::server;
    using recvVar::client;
    using recvVar::window_size;
    int total_recv_bytes = 0;

    // BUFFSIZE至少是window_size的两倍
    const size_t BUFFSIZE = 2*window_size;
    // std::cerr <<"recv> Buffsize = " << BUFFSIZE << std::endl;

    // std::string debug_filename = "recv> filename = ";
    // debug_filename += filename ;
    // debug_filename += "\n";
    // std::cerr << debug_filename;
    
    rtp_packet_t** pack_buf = new rtp_packet_t*[BUFFSIZE]();
    for(size_t i=0;i<BUFFSIZE;i++) pack_buf[i] = nullptr;

    FILE* fp = fopen(filename, "wb");
    // clock_t timer = clock();
    struct timeval timer, now;
    gettimeofday(&timer, NULL);
    size_t firstneedrecv = 0;
    // 设置超时
    // 10s未收到任何东西就结束
    struct timeval timeout;
    timeout.tv_sec = 10;
    timeout.tv_usec = 0;
    while(1) {
        // 单次接收10s的上限
        rtp_packet_t* newpack = revRTP(sock, &client, recvVar::rcvbuf);

        // #ifdef DEBUG
        // if(newpack != nullptr) {
        //     std::cerr<< "recv> type = " << (int)newpack->rtp.type << " seq = " << newpack->rtp.seq_num << '\n';
        // }
        // #endif

        if(newpack != nullptr) { // 收到完好的包
            // 收到了了东西，重启计时器
            gettimeofday(&timer, NULL);
            if(newpack->rtp.type == RTP_END) {
                // std::cerr << "recv> rcv End\n";
                rtp_packet_t* ACK = new rtp_packet_t(RTP_ACK, newpack->rtp.seq_num, nullptr);
                sendRTP(sock, ACK, client);
                break;
            }
            if(newpack->rtp.type == RTP_DATA) {

                uint32_t packseq = newpack->rtp.seq_num;
                // 不合理的seqnum
                if(packseq < firstneedrecv || packseq >= firstneedrecv + window_size) continue ;
                uint32_t buffidx = packseq % BUFFSIZE;
                // std::cerr << "recv> newpack: seq =  " << packseq << " buff_idx = " << buffidx << " firstneed = " << firstneedrecv << " len = " << newpack->rtp.length << "\n";
                // std::cerr << "recv> now =  " << now.tv_sec << "." << now.tv_usec << "\n";
                if(pack_buf[buffidx] == nullptr) pack_buf[buffidx] = newpack;
                while(pack_buf[firstneedrecv%BUFFSIZE] != nullptr) {
                
                    // std::cerr << "recv> Download: seq =  " << firstneedrecv << "\n";
                    Download(pack_buf, firstneedrecv%BUFFSIZE, fp);
                    firstneedrecv ++ ;
                }

                // ACK 第一个需要传的包
                rtp_packet_t* ACK = new rtp_packet_t(RTP_ACK, firstneedrecv, nullptr);
                sendRTP(sock, ACK, client);
            }

        }
        // 10s内未受到任何东西就退出
        gettimeofday(&now, NULL);
        if(now.tv_sec - timer.tv_sec > 10) break; 
    }
    fclose(fp);
    terminateReceiver();
    return total_recv_bytes;
}

int recvMessageOpt(char* filename) {
    std::cerr << "recv> Begin rcvMessage\n";
    using recvVar::sock;
    using recvVar::server;
    using recvVar::client;
    using recvVar::window_size;
    int total_recv_bytes = 0;

    // BUFFSIZE至少是window_size的两倍
    const size_t BUFFSIZE = 2*window_size;
    // std::cerr <<"recv> Buffsize = " << BUFFSIZE << std::endl;

    std::string debug_filename = "recv> filename = ";
    debug_filename += filename ;
    debug_filename += "\n";
    std::cerr << debug_filename;
    
    rtp_packet_t** pack_buf = new rtp_packet_t*[BUFFSIZE]();
    for(size_t i=0;i<BUFFSIZE;i++) pack_buf[i] = nullptr;

    FILE* fp = fopen(filename, "wb");
    // clock_t timer = clock();
    struct timeval timer, now;
    gettimeofday(&timer, NULL);
    size_t firstneedrecv = 0;
    // 设置超时
    // 10s未收到任何东西就结束
    struct timeval timeout;
    timeout.tv_sec = 10;
    timeout.tv_usec = 0;
    while(1) {
        // 单次接收10s的上限
        rtp_packet_t* newpack = revRTP(sock, &client, recvVar::rcvbuf);

        // #ifdef DEBUG
        // if(newpack != nullptr) {
        //     std::cerr<< "recv> type = " << (int)newpack->rtp.type << " seq = " << newpack->rtp.seq_num << '\n';
        // }
        // #endif

        if(newpack != nullptr) { // 收到完好的包
            
            // 重启timer
            gettimeofday(&timer, NULL);
            if(newpack->rtp.type == RTP_END) {
                // std::cerr << "recv> rcv End\n";
                rtp_packet_t* ACK = new rtp_packet_t(RTP_ACK, newpack->rtp.seq_num, nullptr);
                sendRTP(sock, ACK, client);
                break;
            }
            if(newpack->rtp.type == RTP_DATA) {

                uint32_t packseq = newpack->rtp.seq_num;
                // 不合理的seqnum
                if(packseq < firstneedrecv || packseq >= firstneedrecv + window_size) continue ;
                uint32_t buffidx = packseq % BUFFSIZE;
                // std::cerr << "recv> newpack: seq =  " << packseq << " buff_idx = " << buffidx << " firstneed = " << firstneedrecv << " len = " << newpack->rtp.length << "\n";
                // std::cerr << "recv> now =  " << now.tv_sec << "." << now.tv_usec << "\n";
                if(pack_buf[buffidx] == nullptr) pack_buf[buffidx] = newpack;
                while(pack_buf[firstneedrecv%BUFFSIZE] != nullptr) {
                    Download(pack_buf, firstneedrecv%BUFFSIZE, fp);
                    firstneedrecv ++ ;
                }

                // ACK seq与受到的报文seq相同
                rtp_packet_t* ACK = new rtp_packet_t(RTP_ACK, newpack->rtp.seq_num, nullptr);

                sendRTP(sock, ACK, client);
            }

        }
        // 10s内未受到任何东西就退出
        gettimeofday(&now, NULL);
        if(now.tv_sec - timer.tv_sec > 10) break; 
    }
    fclose(fp);
    terminateReceiver();
    return total_recv_bytes;

}

void terminateReceiver() {
    using recvVar::sock;
    // using recvVar::server;
    // using recvVar::client;
    close(sock);
}
