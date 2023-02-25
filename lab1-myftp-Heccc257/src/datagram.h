#ifndef DATAGRAM_H
#define DATAGRAM_H

#include <stdio.h>
#include <iostream>
#include <memory>

#define OPEN_CONN_REQUEST byte(0xa1)
#define OPEN_CONN_REPLY byte(0xa2)
#define AUTH_REQUEST byte(0xa3)
#define AUTH_REPLY byte(0xa4)
#define LIST_REQUEST byte(0xa5)
#define LIST_REPLY byte(0xa6)
#define GET_REQUEST byte(0xa7)
#define GET_REPLY byte(0xa8)
#define PUT_REQUEST byte(0xa9)
#define PUT_REPLY byte(0xaa)
#define QUIT_REQUEST byte(0xab)
#define QUIT_REPLY byte(0xac)

#define FILE_DATA byte(0xff)

#define unused byte(0)
#define sta0 byte(0)
#define sta1 byte(1)

namespace DATAGRAM{
    using byte = std::byte;
    using uint32_t = std::uint32_t;
    using status = byte;
    using type = byte;
    using std::size_t;

    #define MAGIC_NUMBER_LENGTH 6
    #define HEADER_SIZE uint32_t(12)

    struct Header {
        std::byte m_protocol[MAGIC_NUMBER_LENGTH] = {(byte)'\xe3', (byte)'m', (byte)'y', (byte)'f', (byte)'t', (byte)'p'}; /* protocol magic number (6 bytes) */
        type m_type;                          /* type (1 byte) */
        status m_status;                      /* status (1 byte) */
        uint32_t m_length;                    /* length (4 bytes) in Big endian*/
        Header();
        Header(type , status , uint32_t );
        Header(Header *);
        // ~Header();

    } __attribute__ ((packed));

    struct datagram {
        Header *hd;
        byte *payload;
        datagram();
        datagram(type , status , const char*);
        datagram(type , status, size_t, byte*);
        datagram(type, const char*);
        datagram(type , status);
        ~datagram();
        byte Type();
        // void setStatus(byte);
        status getStatus();
        size_t payload_length();
        size_t dg_length();
    };
    datagram* bytes2dgram(byte *);
    datagram* bytes2dgram(byte *);
    void dgram2bytes(byte *, datagram* );
    
    void Debug_datagram(datagram&);
    // datagrams
    datagram* open_conn_request();
    datagram* open_conn_reply(byte );
    datagram* auth_request(const char* );
    datagram* auth_reply(byte );
    datagram* auth_reply(bool );
    datagram* list_request();
    datagram* list_reply(const char *);
    datagram* quit_request();
    datagram* quit_reply();
    datagram* get_request(const char*);
    datagram* get_reply(byte);
    datagram* get_reply(bool);
    datagram* put_request(const char*);
    datagram* put_reply();
    datagram* file_data(size_t, byte*);
}
#endif