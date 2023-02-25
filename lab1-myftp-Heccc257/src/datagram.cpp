#include <stdio.h>
#include <iostream>
#include <cstring>

#include "datagram.h"
#include "util.h"

namespace DATAGRAM {
    Header::Header() { }
    Header::Header(type _type, status _status, uint32_t _length) 
            :m_type(_type), m_status(_status), m_length(endianRev(_length)) { }
    Header::Header(Header *rhs) {
        m_type = rhs->m_type;
        m_status = rhs->m_status;
        m_length = rhs->m_length;
    }
    // Header::~Header() { delete [] m_protocol;}

// byte*不能用sizeof
//*************************datagram*********************    
    datagram::datagram() { }
    datagram::datagram(type _type, status _status, const char *_pl) {
        hd = new Header(_type, _status, HEADER_SIZE + strlen(_pl) + 1);
        if(_pl != NULL) {
            size_t sz = strlen(_pl) + 1;
			payload = new byte[sz];
			memcpy(payload, (byte *)_pl, sz);
		} else payload = nullptr;
    }
    datagram::datagram(type _type, status _status, size_t len, byte *_pl) {
        hd = new Header(_type, _status, HEADER_SIZE + len);
        if(_pl != NULL) {
			payload = new byte[len];
			memcpy(payload, _pl, len);
		} else payload = nullptr;
    }

    datagram::datagram(type _type, const char *_pl) {
        hd = new Header(_type, status(0), HEADER_SIZE + strlen(_pl) + 1);
        if(_pl != NULL) {
            size_t sz = strlen(_pl) + 1;
			payload = new byte[sz];
			memcpy(payload, (byte *)_pl, sz);
		} else payload = nullptr;
    }

	datagram::datagram(type _type, status _status) {
		hd = new Header(_type, _status, HEADER_SIZE); 
		payload = nullptr;
	}
    datagram::~datagram() {
		delete hd;
        if(payload != NULL) delete []payload;
    }

	byte datagram::Type() { return hd->m_type; }
	// void datagram::setStatus(byte sta) { hd->m_status = sta; }
    size_t datagram::payload_length() {
        if(hd == nullptr) return 0;
        else return endianRev(hd->m_length) - HEADER_SIZE;
    }
    size_t datagram::dg_length() {
        if(hd == nullptr) return 0;
        else return endianRev(hd->m_length);
    }
    byte datagram::getStatus() { return hd->m_status; }
    
//*************************datagram*********************    
    // convert byte* to dgram
    // only Head
    // 还需要加载payload
    datagram* bytes2dgram(byte *buff) {
        datagram *dg = new datagram;
        dg->hd = new Header( (Header*)((void*)buff) );
        size_t len = dg->payload_length();
        if(len) {
            dg->payload = new byte[len];
            // memcpy(dg->payload, buff+12, len);
        }
        return dg;
    }

    // convert dgram to byte*
    void dgram2bytes(byte *buff, datagram* dg) {
        memcpy(buff, (byte*)dg->hd, HEADER_SIZE);
        memcpy(buff+HEADER_SIZE, dg->payload, dg->payload_length());
    }
    void Debug_datagram(datagram& dg) {
		Header hd = *dg.hd;
        
        std::cerr << "m_type: " << std::hex << int(hd.m_type)<<"\n";
        std::cerr << "m_status: " << std::hex << int(hd.m_status)<<"\n";
        std::cerr << "m_length: " << std::dec << int(endianRev(hd.m_length))<<"\n";
        if(dg.payload != nullptr) std::cerr << "payload: " << (char*)dg.payload <<"\n";
    }
    // datagrams
    datagram* open_conn_request() { return new datagram(OPEN_CONN_REQUEST, unused); }
    datagram* open_conn_reply(byte status) { return new datagram(OPEN_CONN_REPLY, status); }
    datagram* auth_request(const char* auth) { return new datagram(AUTH_REQUEST, auth); }
    datagram* auth_reply(byte pass) { return new datagram(AUTH_REPLY, pass); }
    datagram* auth_reply(bool pass) { return new datagram(AUTH_REPLY, byte(pass) ); }
    datagram* list_request() { return new datagram(LIST_REQUEST, unused); }
    datagram* list_reply(const char *list) { return new datagram(LIST_REPLY, unused, list); }
    datagram* quit_request() { return new datagram(QUIT_REQUEST, unused); }
    datagram* quit_reply() { return new datagram(QUIT_REPLY, unused); }
    datagram* get_request(const char* filename) { return new datagram(GET_REQUEST, unused, filename); }
    datagram* get_reply(byte status) { return new datagram(GET_REPLY, status); }
    datagram* get_reply(bool status) { return new datagram(GET_REPLY, byte(status)); }
    datagram* put_request(const char* filename) { return new datagram(PUT_REQUEST, unused, filename); }
    datagram* put_reply() { return new datagram(PUT_REPLY, unused); }
    datagram* file_data(size_t len, byte* fileBuf) { return new datagram(FILE_DATA, unused, len, fileBuf); }
}