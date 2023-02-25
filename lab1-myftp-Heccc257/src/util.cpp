#include "util.h"
#include <iostream>
#include <vector>
#include <cstring>
#include <sstream>
#include <memory>

std::uint32_t endianRev(std::uint32_t a) {
    u_char *p = (u_char *)&a;
    return ( (std::uint32_t)(*p)<<24 ) + ( (std::uint32_t)*(p+1)<<16 ) + ( (std::uint32_t)*(p+2)<<8 ) + ( (std::uint32_t)*(p+3) );
}

std::vector<std::string>* Parser_cmd() {
    std::string cmd;
    std::getline(std::cin, cmd);
    std::istringstream ss(cmd);
    std::string tem;
    std::vector<std::string>* p = new std::vector<std::string>();
    while(ss>>tem) { p->push_back(tem); }
    return p;
}

void bytes2chars(std::byte* bbuff, char *cbuff) {
    std::size_t sz = std::strlen(cbuff) + 1;
    bbuff = new std::byte[sz];
    std::memcpy(bbuff, cbuff, sz);
}

void chars2bytes(char *cbuff, std::byte* bbuff, size_t sz) {
    std::memcpy(cbuff, bbuff, sz);
}

namespace myFILE {
    
    using std::byte;
    char* buff = new char[4096];
    size_t fileLength;
    const int FILEBUFSIZE = 1<<21;
    byte* fileBuf = new byte[FILEBUFSIZE];
    std::vector<std::string>* files = new std::vector<std::string>();
    void getFileList() {
        FILE* fp = popen(" ls ", "r");
        if(fp != NULL) {
            int num = fread(buff, sizeof(char), 2048, fp);
            std::cout<<"file = \n"<<buff;
        }
    }
    bool upload(const char* filepath) {
        // std::cerr<<"myFile: filepath "<<filepath<<"\n";
        // std::cerr<<"file = "<<filepath<<"\n";
        FILE *fp = fopen(filepath, "rb");
        // std::cerr<<"myFile: finish "<<filepath<<"\n";
        // std::cerr<<"null  "<<(fp == nullptr)<<"\n";
        if(fp == nullptr) return 0;
        fileLength = 0;
        while(!feof(fp)) {
            size_t ret = fread(fileBuf, sizeof(byte), FILEBUFSIZE - fileLength, fp);
            // std::cerr<<"ret = "<<ret<<"\n";
            fileLength += ret;
        }
        // std::cerr<<"myFile: "<<fileLength<<"\n";
        fclose(fp);
        return 1;
    }
    void download(const char* filepath, byte* file, size_t len) {
        FILE *fp = fopen(filepath, "wb");
        fwrite(file, sizeof(byte), len, fp);
        fclose(fp);
    }
}