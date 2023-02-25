#ifndef DATATRANS_H
#define DATATRANS_H

#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "datagram.h"

using namespace std;

namespace DT {
    using DATAGRAM::datagram;
    using DATAGRAM::bytes2dgram;
    using DATAGRAM::dgram2bytes;
    extern const size_t BUFFSIZE ;
    extern byte *bbuff;
    void writeDgram(int , datagram*);
    datagram* readDgram(int );
}

#endif