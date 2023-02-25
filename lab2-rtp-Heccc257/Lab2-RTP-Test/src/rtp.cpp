#include "rtp.h"
#include "util.h"

#include <cstring>
#include <iostream>

rtp_header_t::RTP_header(): type(0), length(0), seq_num(0), checksum(0) { }
rtp_header_t::RTP_header(uint8_t _type, uint16_t _length, uint32_t _seq)
    :type(_type), length(_length), seq_num(_seq), checksum(0) { }


rtp_packet_t::RTP_packet(uint8_t _type, uint16_t _length, uint32_t _seq, const char* _pl=nullptr)
    :rtp(_type, _length, _seq) {
        
        if(_pl != nullptr) {
            memcpy(payload, _pl, _length);
            payload[_length] = 0;
        } else payload[0] = 0;
        // if(_type == RTP_DATA && _seq == 513) {
        //     std::cerr << "send> constructor checksum = " <<  compute_checksum(this, rtp.length + HeaderSize) << '\n';
        //     std::cerr << "type = " << (int)rtp.type << " len = " <<rtp.length << " seq_num = " << rtp.seq_num << " checksum = " <<rtp.checksum << '\n';
        //     std::cerr << "headear checksum = " << compute_checksum(this, HeaderSize) << " payload checksum = " <<compute_checksum(this->payload, rtp.length)<<'\n';
        // //     exit(0);
        // }
        rtp.checksum = compute_checksum(this, rtp.length + HeaderSize);
    }
rtp_packet_t::RTP_packet(uint8_t _type, uint32_t _seq, const char* _pl=nullptr) {
    rtp.type = _type;
    rtp.seq_num = _seq;
    rtp.checksum = rtp.length = 0;
    if(_pl != nullptr) {
        rtp.length = strlen(_pl);
        memcpy(payload, _pl, strlen(_pl));
        payload[rtp.length] = 0;
    } else payload[0] = 0;
    rtp.checksum = compute_checksum(this, rtp.length + HeaderSize);
}

rtp_packet_t::RTP_packet(const rtp_header_t& _rtp , const char* _pl=nullptr) 
    :rtp(_rtp) {
        if(_pl != nullptr) {
            memcpy(payload, _pl, strlen(_pl));
            payload[strlen(_pl)] = 0;
        } else payload[0] = 0;
        rtp.checksum = compute_checksum(this, rtp.length + HeaderSize);
    }
    
bool rtp_packet_t::Damaged() {
    uint32_t checksum = rtp.checksum;
    // 计算checksum的时候checksum上的数字还是0
    rtp.checksum = (uint32_t)0;

    // std::cerr<<"func Damaged: seq = " << rtp.seq_num <<" len = " << rtp.length << " type = " << (int)rtp.type << " .checksum=" << checksum << " rtp.checksum = " << rtp.checksum << ' ' << compute_checksum(this, rtp.length + HeaderSize)<<'\n';
    // std::cerr << "type = " << (int)rtp.type << " len = " <<rtp.length << " seq_num = " << rtp.seq_num << " checksum = " <<rtp.checksum << '\n';
    // std::cerr << "headear checksum = " << compute_checksum(this, HeaderSize) << " payload checksum = " <<compute_checksum(this->payload, rtp.length)<<'\n';

    bool res = (checksum != compute_checksum(this, rtp.length + HeaderSize));
    rtp.checksum = checksum;
    // std::cerr<<"func Damaged res = " << res << '\n';
    return res;
}