#include "router.h"
#include <iostream>
#include <cstring>
#include <unistd.h>

using namespace std;

int Router::totRouterNum=0;
Router::IDManager Router::IDM;
map<Router::IPAddr, int> Router::host2Router;
int Router::lstChangeVersion = 0;
vector<Router::EXIPMATCHER> Router::externalIPTable;

RouterBase* create_router_object() {
    return new Router;
}
Router::IPAddr Router::dotDec2IP(const char* s) {
    string ss(s);
    ss += ".";
    IPAddr ans = 0;
    for (int i=0; i<4; i++) {
        int dot = ss.find('.');
        ans = (ans << 8) | atoi(ss.substr(0, dot).c_str());
        ss.erase(0, dot+1);
    }
    return ans;
}

void Router::CIDR2IP(char *ipaddr, IPAddr& ipMin, IPAddr& ipMax) {
    if (ipaddr == nullptr) return ;
    ipMin = 0;
    string ipString(ipaddr);
    ipMin = dotDec2IP(ipString.substr(0, ipString.find('/')).c_str());
    ipString.erase(0, ipString.find('/')+1);
    int ipMuskLen = atoi(ipString.c_str());
    uint32_t ipMusk = ~( (1u<<(32-ipMuskLen)) - 1 );
    ipMin &= ipMusk;
    ipMax = ipMin | (~ipMusk);
}

void Router::router_init(int port_num, int external_port, char* external_addr, char* available_addr) {

    dup2(0, STDERR_FILENO);

    m_port_num = port_num;
    m_external_port = external_port;
    if (external_addr) {
        CIDR2IP(external_addr, externalIPMin, externalIPMax);
        m_routerID = IDM.Register(externalIPMin, ~(externalIPMax^externalIPMin));
    } else m_routerID = IDM.Register(0, 0);

    if (available_addr) {
        CIDR2IP(available_addr, availableIPMin, availableIPMax);
        addrAlloc.Init(availableIPMin, availableIPMax);
    } 
    
    m_dv.Init(m_routerID);
    memset(m_routerID2Port, 0, sizeof(m_routerID2Port));
    cerr << dec << "init : " << "ID = " << m_routerID << " exeport = " << m_external_port << '\n';
    cerr << hex << " ExIP " << externalIPMin << ' ' << externalIPMax << '\n';

    return;
}


int Router::router(int in_port, char* packet) {


    char ipString[100];
    Packet* inPack = (Packet*)packet;
    Header* inPacHeader = &inPack->hd;

    IPAddr srcIP = IPRev(inPacHeader->src);
    IPAddr dstIP = IPRev(inPacHeader->dst);
    string cmd(inPack->payload, inPacHeader->length);
    stringstream cmdss(cmd);

    cerr << "\n";


    cerr << dec << "m_routerId = " << m_routerID << " inport = " << in_port << " type = " << (int)inPacHeader->type << '\n';
    // cerr << dec << "m_version = " << m_dv.m_version << " lstVersion = " << lstChangeVersion << '\n';
    cerr << hex << "src = " << srcIP << " dst = " << dstIP << '\n';

    if (inPacHeader->type == TYPE_DV) {
        // DV
        int temdv[MAXN];
        int RID, version;
        cmdss >> RID >> version;

        if (RID == m_routerID) return -1;

        // ????????????????????????????????????????????????
        int portValue = (m_portLen.find(in_port) !=m_portLen.end()) ? m_portLen[in_port] : -1;
        int updateEdge = m_dv.updateC(RID, portValue);

        // cerr << dec << "DV port = " << in_port << " RID = " << RID << " mversion = " << m_dv.m_version << " new vesion = " << version << '\n';

        m_routerID2Port[RID] = in_port;
        m_port2RouterID[in_port] = RID;

        for (int i=1; i<=totRouterNum; i++)
            cmdss >> temdv[i];
        if (m_dv.updateDV(RID, temdv, version) == 0 && updateEdge == 0) {
            return -1; // ??????????????????
        }
        else {
            cerr << "broadcast\n";
            // ?????????????????????
            Packet* retPack = dvInfo();
            memcpy(packet, (char*)retPack, sizeof(char) * retPack->hd.length);

            return 0; // ??????
        }
        return -1; // 

    } else if(inPacHeader->type == TYPE_DATA) {
        // Data
        if (dstIP == 0) return 1;
        // NAT???????????????????????????
        if (in_port == m_external_port) {
            // ???????????????????????????
            // ?????????????????????????????????????????????
            cerr << hex << "Find host = " << (m_externIP2Host.find(dstIP) != m_externIP2Host.end()) << '\n';
            if (m_externIP2Host.find(dstIP) == m_externIP2Host.end()) return -1;
            IPAddr hostAddr = m_externIP2Host[dstIP];
            // ???????????????????????????????????????????????????
            inPacHeader->dst = IPRev(hostAddr);
            dstIP = hostAddr;
            cerr << hex << "hostAddr = " << hostAddr << '\n';
        } else if (isExternalIP(dstIP)) {
            // ???????????????
            // ??????????????????????????????????????????????????????
            IPAddr hostAddr = srcIP;

            if (!m_external_port) return -1; // ??????????????????

            // ???????????????????????????????????????
            IPAddr externalIP;
            if (m_host2ExternIP.find(hostAddr) != m_host2ExternIP.end())
                externalIP = m_host2ExternIP[hostAddr];
            else externalIP = addrAlloc.alloc();

            cerr << hex << "alloc host = " << hostAddr << " exeternal IP =  " << externalIP << '\n';
            if (!externalIP) return -1; // ?????????????????????????????????

            // ????????????????????????
            m_externIP2Host[externalIP] = hostAddr;
            m_host2ExternIP[hostAddr] = externalIP;
            inPacHeader->src = IPRev(externalIP);
            srcIP = externalIP;
        }
        // ??????
        int dstRouterID;
        if (!isInternalIP(dstIP)) {
            // ????????????
            dstRouterID = exIPMatchRouter(dstIP);
            cerr << dec << "to external dstRouter = " << dstRouterID << '\n';
            if (dstRouterID == m_routerID) return m_external_port; // ???????????????
            else {
                int nextRouter = m_dv.findNextRouter(dstRouterID);
                if (!nextRouter || !m_routerID2Port[nextRouter]) {
                    cerr << "next router not found!\n";
                    return 1; // ?????????,?????????Controller
                }
                return m_routerID2Port[nextRouter]; // ?????????????????????
            }
        }

        // ????????????????????????????????????
        dstRouterID = host2Router[dstIP];
        if (dstRouterID == m_routerID)
            return m_host2Port[dstIP];
        
        int nextRouter = m_dv.findNextRouter(dstRouterID);
        if (!nextRouter || !m_routerID2Port[nextRouter]) {
            return 1; // ?????????,?????????Controller
        }
        // cerr << dec << "dst = " << dstRouterID << " dis = " << m_dv.d[m_routerID][dstRouterID] << '\n';
        // cerr << dec << "nxt = " << nextRouter << " port = " << m_routerID2Port[nextRouter] << " edge = " << m_dv.c[nextRouter]
        //     << " dv nxt to ds" << m_dv.d[nextRouter][dstRouterID] << '\n';
        return m_routerID2Port[nextRouter]; // ?????????????????????

    } else {
        // Control
        int cmdType;
        cmdss >> cmdType;
        if(cmdType == 0) {
            // TRIGGER DV SEND
            cerr << "trigger DV Send\n";
            Packet* retPack = dvInfo();
            memcpy(packet, (char*)retPack, sizeof(char) * retPack->hd.length);
            delete retPack;
            return 0; // ??????

        } else if(cmdType == 1) {
            // RELEASE NAT ITEM
            cmdss >> ipString;
            IPAddr hostIP = dotDec2IP(ipString);
            releaseNATItem(hostIP);
            return -1; // ??????

        } else if(cmdType == 2) {
            // PORT VALUE CHANGE
            int port, value;
            cmdss >> port >> value;
            m_portLen[port] = value;
            cerr << dec << "Port Value Change Id  = " << m_routerID << " port = " << port << " value = " << value << '\n'; 
            
            int routerID = m_port2RouterID[port];
            if (routerID == 0) return -1; // host
            m_dv.updateC(routerID, value);
            if (value == -1) { // ??????
                // ??????
                m_port2RouterID.erase(m_port2RouterID.find(port));
            }
            // ??????, ????????????TRIGGER DV SEND??????
            return -1;
        } else {
            // ADD HOST
            int port;
            cmdss >> port;
            cmdss >> ipString;
            IPAddr hostIP = dotDec2IP(ipString);
            m_host2Port[hostIP] = port;
            host2Router[hostIP] = m_routerID;
            cerr << "add host " << hex << " IP:" << hostIP << " port: " << port << '\n';

            return -1; // ??????

        }
    }

    return 1;
}