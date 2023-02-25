#include "./src/datatrans.h"
#include "./src/util.h"
#include "./src/datagram.h"

#include <iostream>
#include <cstring>
#include <vector>
#include <sstream>
#include <memory>

using namespace std;
// using namespace DATAGRAM;
using namespace DT;


using DATAGRAM::open_conn_reply;
using DATAGRAM::auth_reply;
using DATAGRAM::list_reply;
using DATAGRAM::quit_reply;
using DATAGRAM::get_request;
using DATAGRAM::get_reply;
using DATAGRAM::file_data;
using DATAGRAM::put_reply;

using myFILE::getFileList;
using myFILE::upload;
using myFILE::download;
// using myFILE::getFileList2String;

// char *IP = "127.0.0.1";
// char *port = "12323";

namespace AUTHENCIATION {
    bool auth_pass;
    const char* uid = "user";
    const char* pass = "123123";
    string iuid, ipass;
    // char *iuid = new char[128];
    // char *ipass = new char[128];
    void check(const char *auth) {
        istringstream sstr(auth);
        sstr>>iuid;
        sstr>>ipass;
        auth_pass = (!strcmp(uid, iuid.c_str())) && (!strcmp(pass, ipass.c_str()));
    }
    void init() { auth_pass = 0; }
}

using AUTHENCIATION::auth_pass;
void initConn() {
    AUTHENCIATION::init();
}

int main(int argc, char ** argv) {
    int serv_sock = socket(AF_INET, SOCK_STREAM, 0); // 申请一个TCP的socket
    struct sockaddr_in serv_addr; // 描述监听的地址
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;  //使用IPv4地址

    if(argc > 1) {
        // cerr<<"arg "<<argv[1] <<" "<<argv[2]<<"\n";
        serv_addr.sin_addr.s_addr = inet_addr(argv[1]);  //具体的IP地址
        serv_addr.sin_port = htons(atoi(argv[2]));  //端口
    } else {
        serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");  //具体的IP地址
        serv_addr.sin_port = htons(1234);  //端口
    }

    bind(serv_sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
    listen(serv_sock, 128);

    // cerr<<"IP port"<<serv_addr.sin_addr.s_addr<<" "<<serv_addr.sin_port<<"\n";

    //接收客户端请求
    cerr<<"waiting...\n";
    struct sockaddr_in clnt_addr;
    socklen_t clnt_addr_size = sizeof(clnt_addr);
    int clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_addr, &clnt_addr_size);
    // cerr<<"acc "<<clnt_sock<<"\n";
    cerr<<"connected\n";
    int waitTimesLim = 1e5;
    while(1) {
        // cerr<<"wait for rev dgram\n";
        if(! --waitTimesLim) break;
        datagram* rev = nullptr;
        datagram* reply = nullptr;
        rev = readDgram(clnt_sock);
        // cerr<<"rev: "<<rev->dg_length()<<"\n";
        if(rev->Type() == OPEN_CONN_REQUEST) {
            // openn_conn_reply
            reply = open_conn_reply(sta1);
            writeDgram(clnt_sock, reply);
            initConn();
        } else if(rev->Type() == AUTH_REQUEST) {
            const char* auth = (char*) rev->payload;
            // cerr<<"server auth\n";
            AUTHENCIATION::check(auth);
            // cerr<<"auth sennd\n";
            reply = auth_reply(auth_pass);
            // DATAGRAM::Debug_datagram(*reply);
            writeDgram(clnt_sock, reply);
        } else if(rev->Type() == LIST_REQUEST) {
            getFileList();
            reply = list_reply(myFILE::buff);
            writeDgram(clnt_sock, reply);
        } else if(rev->Type() == QUIT_REQUEST) {
            // cerr<<"quit\n";
            reply = quit_reply();
            writeDgram(clnt_sock, reply);
            // cerr<<"quit end\n";
            break;
        } else if(rev->Type() == GET_REQUEST) {
            // cerr<<"get\n";
            const char* filename = (const char*)rev->payload;
            const char* filepath = filename;
            bool exisit = upload(filepath);
            // reply
            reply = get_reply(exisit);
            writeDgram(clnt_sock, reply);
            if(exisit) {
                // upload file
                cerr<<"get loading\n";
                datagram* fileDgram = file_data(myFILE::fileLength, myFILE::fileBuf);
                writeDgram(clnt_sock, fileDgram);
                delete fileDgram;
            }
        } else if(rev->Type() == PUT_REQUEST) {
            const char* filename = (char*)rev->payload;
            const char* filepath = filename;
            // reply
            reply = put_reply();
            writeDgram(clnt_sock, reply);
            // file
            datagram* fileDgram = readDgram(clnt_sock);
            myFILE::download(filepath, fileDgram->payload, fileDgram->payload_length());
            
        }
        if(rev != NULL) delete rev;
        if(reply != NULL) delete reply;
    }
    // // 读取
    // byte serv_rd[48];
    // printf("from client: %s\n",serv_rd);

    // //向客户端发送数据
    // char str[] = "Hello World!";
    // ssize_t wlen = write(clnt_sock, str, sizeof(str));

    // //关闭套接字
    close(clnt_sock);
    close(serv_sock);

    return 0;
}
/*
./ftp_server 127.0.0.1 12323
../build/ftp_server 127.0.0.1 12323
../build/ftp_server 172.27.64.1 12323
*/