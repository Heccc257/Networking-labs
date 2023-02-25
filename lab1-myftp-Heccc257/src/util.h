#ifndef UTIL_H
#define UTIL_H

#include <iostream>
#include <vector>
#include <cstring>
#include <sstream>
#include <memory>
#include <filesystem>
#include <stdio.h>
#include <stdlib.h>

namespace fs = std::filesystem;

// #include <io.h>

std::uint32_t endianRev(std::uint32_t );

std::vector<std::string>* Parser_cmd();

void bytes2chars(std::byte* , char *);

void chars2bytes(char *, std::byte* , size_t );


namespace myFILE {
    using std::byte;
    using std::size_t;
    extern char* buff;
    
    extern const int FILEBUFSIZE;
    extern byte* fileBuf;
    extern size_t fileLength;
    extern std::vector<std::string>* files;
    void getFileList();
    // store file to fileBuf
    // return length
    bool upload(const char*);
    void download(const char*, byte*, size_t);
}


#endif