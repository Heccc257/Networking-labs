#include "datatrans.h"
#include <cstring>

namespace DT {

    const size_t BUFFSIZE = 1<<22;
    byte *bbuff = new byte[BUFFSIZE];

    void writeBuf(int tgt_socket, byte* buf, size_t len) {
        size_t ret = 0;
        while(ret < len) {
            size_t b = write(tgt_socket, buf + ret, len - ret);
            ret += b;
        }
    }
    void readBuf(int src_socket, byte* buf, size_t len) {
        size_t ret = 0;
        while(ret < len) {
            size_t b = read(src_socket, buf + ret, len - ret);
            ret += b;
        }
    }
    
    void writeDgram(int tgt_socket, datagram* dgram) {
        dgram2bytes(bbuff, dgram);
        size_t len = dgram->dg_length();
        writeBuf(tgt_socket, bbuff, len);
    }
    datagram* readDgram(int src_socket) {
        readBuf(src_socket, bbuff, HEADER_SIZE);
        datagram* reply = bytes2dgram(bbuff); // only Head
        // read payload
        if(reply->payload_length()) {
            readBuf(src_socket, reply->payload, reply->payload_length());
        }
        return reply;
    }
}