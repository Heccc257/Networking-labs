#include <unistd.h>//close
#include <arpa/inet.h>//网络字节序和主机字节序库转换
#include <stdlib.h>//atoi
#include <netinet/in.h>
#include <string.h>
#include <time.h>
#include <vector>
#include <iostream>
#include <sys/time.h>

#include "sender_def.h"
#include "rtp.h"
#include "datatrans.h"
#include "util.h"

namespace senderVar {
    int sock;
    struct sockaddr_in server;
    rtp_packet_t* conEnd;
    // 未确认的报文数量
    uint32_t window_size;
    const int BUFFSIZE = 2048;
    char rcvbuf[BUFFSIZE];
}
int initSender(const char* receiver_ip, uint16_t receiver_port, uint32_t window_size) {
    using senderVar::sock;
    using senderVar::server;
    using senderVar::conEnd;

    senderVar::window_size = window_size;

    // 创建套接字
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if(sock < 0) return -1;
    // std::cerr << "sender socket OK!\n";


    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(receiver_port);
    server.sin_addr.s_addr = inet_addr(receiver_ip);

    // random　seq
    uint32_t seq = ((uint32_t)rand() * rand()) & ((1u<<31)-1);
    std::cerr<<"send> seq = " << seq << "\n";
    rtp_packet_t* rtpStart = new rtp_packet_t(RTP_START, seq, nullptr);
    
    // usleep(100000);
    // Start
    if( !sendRTP(sock, rtpStart, server) ) {
        // Start报文传输失败 
        std::cerr << "send> rptStart 传输失败\n";

        close(sock);
        return -1;
    }

    std::cerr << "send> rptStart sended\n";

    std:: cerr << "send> waiting for start ACK!\n";
    rtp_packet_t* ACK = nullptr;

    // 设置超时
    struct timeval timeout;
    timeout.tv_sec = 2;
    timeout.tv_usec = 0;
    if( setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0 ) {
        std::cerr << "send> setsocketopt failed\n";
    } else {
        std::cerr << "send> setsocketopt sucess\n";
    }
    // 2s超时
    ACK = revRTP(sock, &server, senderVar::rcvbuf);

    if(ACK == nullptr || 
        ACK->rtp.type != RTP_ACK || 
        ACK->rtp.seq_num != seq) {

        // 不正确的ACK报文
        std::cerr<<"send> ACK error\n";
        conEnd = new rtp_packet_t(RTP_END, seq, nullptr);
        terminateSender();
        return -1;
    }

    std::cerr<<"send> connect success!!\n";
    return 0;
}

int sendMessage(const char* message) {
    std::cerr << "send> Begin sendMessage\n";
    using std::vector;
    using std::pair;
    using std::make_pair;
    using senderVar::sock;
    using senderVar::server;
    using senderVar::conEnd;
    using senderVar::window_size;

    uint32_t seq_num = 0; // seq_num从0开始

    vector< rtp_packet_t* > packs;

    std::cerr << "send> message = " << message << '\n';
    FILE* fp = fopen(message, "rb");
    char* messbuf = new char[PAYLOAD_SIZE]();
    if(fp == nullptr) {
        std::cerr << "send> no such file\n";
        return -1;
    }
    // 将message分割
    // for(size_t i=0; i<mesLen; i += PAYLOAD_SIZE-1) 
    //     seqs.push_back( make_pair(i, std::min((size_t)PAYLOAD_SIZE-1, mesLen-i) ) );

    uint32_t base = 0; // 第一个未被确认的seqnum
    uint32_t nextseqnum = 0; // 下一个要传的
    uint32_t totseq = 0; // 总的要传的
// #ifdef DEBUG
    // for(auto pack : packs) {
    //     std::cout << "packet " << pack->rtp.seq_num << " type=" << (int)pack->rtp.type << " " << pack->rtp.length << "\n";
    // }
    // return 0;
// #endif

    // 设置超时
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 100;
    if( setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0 ) {
        std::cerr << "send> setsocketopt failed\n";
    } else {
        std::cerr << "send> setsocketopt sucess\n";
    }

    // clock_t timer = clock();
    struct timeval timer, now;
    gettimeofday(&timer, NULL);
    while(1) {
        // 先从文件中读取
        while(totseq < base + window_size) {
            uint16_t len = fread(messbuf, 1, PAYLOAD_SIZE-1, fp);
            if(!len) break; // 已经读完了
            // std::cerr << "send> read " << len << " from message\n";
            packs.push_back(
                new RTP_packet(RTP_DATA, len, seq_num++, messbuf)
            );
            totseq ++ ;
        }
        
        // std::cerr<<"send> base = " << base << " tot = " << totseq << '\n';
        if(base == totseq) break;
        while(nextseqnum < totseq && nextseqnum < base + window_size) {
            // std::cerr << "send> first send" << "base = " << base << " next = " << nextseqnum << "\n";
            // 不保证可靠传输
            sendRTP(sock, packs[nextseqnum], server);
            nextseqnum ++;
            // restart timer
            // timer = clock();
            gettimeofday(&timer, NULL);
        }
        // std::cerr<<"send> waiting for ACK\n";
        rtp_packet_t *ACK = revRTP(sock, &server, senderVar::rcvbuf);

        if(ACK != nullptr && ACK->rtp.type == RTP_ACK) {
            // std::cerr << "send> rcv ACK " << ACK->rtp.seq_num << '\n';
            if(ACK->rtp.seq_num > base) {
                for(int i=base; i<ACK->rtp.seq_num; i++) delete packs[i];
                // ACK的seqnum为下一个需要传输的seqnum
                // 更新base
                base = ACK->rtp.seq_num;
                // restart timer
                // timer = clock();
                gettimeofday(&timer, NULL);
            }
        }

        gettimeofday(&now, NULL);

        // 微秒为单位
        unsigned int time_microsec = (now.tv_sec - timer.tv_sec) * 1000 + (now.tv_usec - timer.tv_usec) / 1000;
        if( time_microsec > 100 ) {
            // 100ms未收到ACK
            // windowsize 内全部重传
            // std::cerr << "send> 重传 now = " << now.tv_usec << "\n";
            for(uint32_t i = base; i<nextseqnum; i++) {
                // std::cerr << "send> i=" << i << " seq = " << packs[i]->rtp.seq_num << '\n';
                sendRTP(sock, packs[i], server);
            }
            // timer = clock();
            gettimeofday(&timer, NULL);
        }

    }
    std::cerr<<"send> file send finish\n";
    conEnd = new rtp_packet_t(RTP_END, packs.size(), nullptr);
    terminateSender();
    return 0;
}

// def conEnd first!!
void terminateSender() {
    using senderVar::sock;
    using senderVar::server;
    using senderVar::conEnd;
    // 没有考虑End 和 ACK 丢包？
    sendRTP(sock, conEnd, server);
    rtp_packet_t* ACK = nullptr;
    ACK = revRTP(sock, &server, senderVar::rcvbuf);
    std::cerr << "send> terminate end ACK " << (ACK == nullptr) << '\n';
    close(sock);
}


int sendMessageOpt(const char* message) {
    std::cerr << "send> Begin sendMessage\n";
    using std::vector;
    using std::pair;
    using std::make_pair;
    using senderVar::sock;
    using senderVar::server;
    using senderVar::conEnd;
    using senderVar::window_size;
    
    uint32_t seq_num = 0; // seq_num从0开始

    vector< rtp_packet_t* > packs;

    const int ACKBUFFSIZE = window_size * 2;
    bool* acked = new bool[ACKBUFFSIZE];
    

    std::cerr << "send> message = " << message << '\n';
    FILE* fp = fopen(message, "rb");
    char* messbuf = new char[PAYLOAD_SIZE]();

    if(fp == nullptr) {
        std::cerr << "send> no such file\n";
        return -1;
    }

    uint32_t base = 0; // 第一个未被确认的seqnum
    uint32_t nextseqnum = 0; // 下一个要传的
    uint32_t totseq = 0; // 总的要传的

    // 设置超时
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 100;
    if( setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0 ) {
        std::cerr << "send> setsocketopt failed\n";
    } else {
        std::cerr << "send> setsocketopt sucess\n";
    }

    // clock_t timer = clock();
    struct timeval timer, now;
    gettimeofday(&timer, NULL);
    while(1) {
        // 先从文件中读取
        while(totseq < base + window_size) {
            uint16_t len = fread(messbuf, 1, PAYLOAD_SIZE-1, fp);
            if(!len) break; // 文件中的内容已经读完了
            // std::cerr << "send> read " << len << " from message\n";
            packs.push_back(
                new RTP_packet(RTP_DATA, len, seq_num++, messbuf)
            );
            totseq ++ ;
        }
        // 所有包都已经传完并且受到ack了
        if(base == totseq) break;
        while(nextseqnum < totseq && nextseqnum < base + window_size) {
            // std::cerr << "send> first send" << "base = " << base << " next = " << nextseqnum << "\n";
            // 不保证可靠传输
            sendRTP(sock, packs[nextseqnum], server);
            acked[nextseqnum%ACKBUFFSIZE] = 0;
            nextseqnum ++;
            // restart timer
            // timer = clock();
            gettimeofday(&timer, NULL);
        }
        // std::cerr<<"send> waiting for ACK\n";
        rtp_packet_t *ACK = revRTP(sock, &server, senderVar::rcvbuf);

        if(ACK != nullptr && ACK->rtp.type == RTP_ACK) {

            // std::cerr << "send> rcv ACK " << ACK->rtp.seq_num << '\n';
            if(ACK->rtp.seq_num >= base && ACK->rtp.seq_num < base + window_size) {
                acked[ACK->rtp.seq_num%ACKBUFFSIZE] = 1;    
            }
            gettimeofday(&timer, NULL);
        }
        while(base < nextseqnum && acked[base%ACKBUFFSIZE] == 1) {
            delete packs[base];
            base++;
        }

        gettimeofday(&now, NULL);

        // 微秒为单位
        unsigned int time_microsec = (now.tv_sec - timer.tv_sec) * 1000 + (now.tv_usec - timer.tv_usec) / 1000;
        if( time_microsec > 100 ) {
            // 100ms未收到ACK
            // windowsize 内重传没有受到ACK的
            for(uint32_t i = base; i<nextseqnum; i++) {
                if(acked[i%ACKBUFFSIZE] == 0)
                    sendRTP(sock, packs[i], server);
            }
            // timer = clock();
            gettimeofday(&timer, NULL);
        }

    }
    std::cerr<<"send> file send finish\n";
    conEnd = new rtp_packet_t(RTP_END, packs.size(), nullptr);
    terminateSender();
    return 0;
}
/*
*/