#include "./src/datatrans.h"
#include "./src/util.h"
#include "./src/datagram.h"

#include <iostream>
#include <cstring>
#include <vector>
#include <sstream>
#include <unistd.h>
#include <filesystem>

using namespace std;
using namespace DT;
namespace fs = std::filesystem;

#define stou16(s) uint16_t(atoi(s.c_str()))

using DATAGRAM::open_conn_request;
using DATAGRAM::auth_request;
using DATAGRAM::list_request;
using DATAGRAM::quit_request;
using DATAGRAM::get_request;
using DATAGRAM::get_reply;
using DATAGRAM::file_data;
using DATAGRAM::put_request;

using myFILE::upload;
using myFILE::download;

enum CMDTYPE {
    UNKNOWN,
    OPEN,
    AUTH,
    LS,
    GET,
    PUT,
    QUIT
};
CMDTYPE CheckCmd(const vector<string>&cmd) {
    if (cmd.size() < 1) return UNKNOWN;
    if (cmd[0] == "open") {
        return OPEN;
    } else if (cmd[0] == "auth") {

        return AUTH;
    } else if (cmd[0] == "auth") {

        return AUTH;
    } else if (cmd[0] == "ls") {

        return LS;
    } else if (cmd[0] == "get") {

        return GET;
    } else if (cmd[0] == "put") {

        return PUT;
    } else if (cmd[0] == "quit") {

        return QUIT;
    } else {
        return UNKNOWN;
    }
}

int ConnectServer(const char *IP, uint16_t port) {
    cerr<<"IP = "<<IP<<"\n";
	cerr<<"client conn\n";
    //创建套接字
	int serv_sock = socket(AF_INET, SOCK_STREAM, 0);
    //向服务器（特定的IP和端口）发起请求
	struct sockaddr_in serv_addr;
	memset(&serv_addr, 0, sizeof(serv_addr));  //每个字节都用0填充
	serv_addr.sin_family = AF_INET;  //使用IPv4地址
    // serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");  //具体的IP地址
    serv_addr.sin_addr.s_addr = inet_addr(IP);  //具体的IP地址
    // serv_addr.sin_port = htons(1234);  //端口
    serv_addr.sin_port = htons(port);  //端口
    int res = connect(serv_sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
    return (!res)?serv_sock : -1;
}


int serv_socket = -1;

bool clnt_conn = 0;
bool clnt_auth = 0;

void connSucess() {
    clnt_conn = 1;
    clnt_auth = 0;
}

void authSucess() {
    puts("Authenciation granted.");
    clnt_conn = 1;
    clnt_auth = 1;
}
void authFail() {
    clnt_conn = 0;
    clnt_auth = 0;
}

// exe相对路径
string exe_dir;

bool Check_auth() {
    if(!clnt_conn || !clnt_auth) {
        puts("Log in first!!");
        return 0;
    }
    return 1;
}

int main(int argc, char ** argv) {
    exe_dir = string(argv[0]);
    exe_dir = exe_dir.substr(0, exe_dir.rfind('/')+1);
    // cerr<<"cur "<< fs::path().string() <<"\n";
    // cerr<<"cur "<<fs::current_path().string()<<"\n";
    // cerr<<"exe_dir: "<<exe_dir<<"\n";
    while(1) {
        cout<<"myFTPClien> ";
        auto p = Parser_cmd();
        vector<string>& cmd = *p;
        CMDTYPE TYPE = CheckCmd(cmd);
        // cerr<<"cmd type "<<cmd[0]<<" "<<TYPE<<"\n";
        datagram* request = nullptr;
        datagram* reply = nullptr;

        if(TYPE == OPEN) {
            if(serv_socket == -1) 
                serv_socket = ConnectServer( cmd[1].c_str(), stou16(cmd[2]) );
            
            if(serv_socket == -1) {
                puts("Server Not Found!!");
            } else {
                // send request
                // cerr<<"send conn request\n";
                request = open_conn_request();
                writeDgram(serv_socket, request);
                reply = readDgram(serv_socket);
                if(reply->Type() == OPEN_CONN_REPLY && reply->getStatus() == sta1) {
                    puts("Server connection accepted.");
                } else {
                    puts("Server connection rejected.");
                }
                connSucess();
                // Debug_datagram(*rev);
            }
        } else if(TYPE == AUTH) {
            if(!clnt_conn) {
                puts("not connected!!!!");
                continue ;
            }
            if(clnt_auth) {
                puts("Auth already!");
                continue ;
            }
            string auth = cmd[1] + " " + cmd[2];
            // cerr<<"auth: "<<auth<<"\n";
            writeDgram(serv_socket, auth_request(auth.c_str()));
            reply = readDgram(serv_socket);
            // DATAGRAM::Debug_datagram(*reply);
            if(reply->Type() == AUTH_REPLY && reply->getStatus() == sta1) {
                // cerr<<"auth sucess\n";
                authSucess();
            } else {
                authFail();
                // 断开连接
                break;
            }
        } else if(TYPE == LS) {
            // cerr<<"clnt ls\n";
            if(!Check_auth()) continue ;
            // cerr<<"clnt ls\n";
            request = list_request();
            writeDgram(serv_socket, request);
            reply = readDgram(serv_socket);
            const char* filenames = (const char*)reply->payload;
            puts("----- file list start -----");
            cout<<filenames;
            puts("----- file list end -----");
            // cerr<<"files: "<<filenames<<"\n";
        } else if(TYPE == QUIT) {
            // cerr<<"quit req\n";
            request = quit_request();
            writeDgram(serv_socket, request);
            // cerr<<"quit reply\n";
            reply = readDgram(serv_socket);
            // DATAGRAM::Debug_datagram(*reply);
            break;
        } else if(TYPE == GET) {
            if(!Check_auth()) continue ;
            // 与client可执行文件在同一目录下
            const char* filename = cmd[1].c_str();
            // cerr<<"get filename "<<filename<<"\n";
            request = get_request(filename);
            writeDgram(serv_socket, request);
            // cerr<<" get reply:\n";
            reply = readDgram(serv_socket);
            // DATAGRAM::Debug_datagram(*reply);
            if(reply->getStatus() == sta0) {
                puts("Server no such file!!");
            } else {
                cerr<<"-----------file begin-----------\n";
                puts("File downloaded.");
                datagram* fileDgram = nullptr;
                while(fileDgram == nullptr) fileDgram = readDgram(serv_socket);
                // cerr<<"get fileDgram:\n";
                // DATAGRAM::Debug_datagram(*fileDgram);

                // const char *filepath = (exe_dir + cmd[1]).c_str();
                const char *filepath = cmd[1].c_str();
                cerr<<"download filename "<<filepath<<"\n";
                myFILE::download(filepath, fileDgram->payload, fileDgram->payload_length());
                delete fileDgram;
            }
            // cerr<<"get end\n";
        } else if(TYPE == PUT) {
            if(!Check_auth()) continue ;
            const char* filename = cmd[1].c_str();
            // const char* filepath = (exe_dir + cmd[1]).c_str();
            const char* filepath = cmd[1].c_str();
            if(!upload(filepath)) {
                puts("FtpClient: File not found.");
            } else {
                cerr<<"filename: "<<filename<<"\n";
                // request
                request = put_request(filename);
                writeDgram(serv_socket, request);
                // replay
                reply = readDgram(serv_socket);
                // file
                // cerr<<"filedgrtam loading"<<"\n";
                datagram* fileDgram = file_data(myFILE::fileLength, myFILE::fileBuf);
                // cerr<<"filedgrtam: "<<fileDgram->payload_length()<<"\n";
                writeDgram(serv_socket, fileDgram);
                puts("File uploaded.");
                delete fileDgram;
            }
        }

        if(request != NULL) delete request;
        if(reply != NULL) delete reply;
        delete p;
    }

    //关闭套接字
    close(serv_socket);

    return 0;
}
/*
../build/ftp_client
open 127.0.0.1 12323
auth user 123123
*/