#include "router_prototype.h"
#include <iostream>
#include <vector>
#include <cstring>
#include <algorithm>
#include <map>
#include <sstream>

class Router : public RouterBase {

public:
    static const int MAXN = 100;
    // header
    static const int maxPacketSize = 16384;
    static const int headerSize = 12;
    static const int maxPayloadSize = maxPacketSize - headerSize;
    struct Header {
        // src dst大端法
        uint32_t    src;
        uint32_t    dst;
        uint8_t     type;
        uint16_t    length;
        Header() { }
        Header(uint32_t _src, uint32_t _dst, uint8_t _type, uint16_t _length)
            : src(_src), dst(_dst), type(_type), length(_length) { }
    };

    static const uint8_t TYPE_DV         = 0x00;
    static const uint8_t TYPE_DATA       = 0x01;
    static const uint8_t TYPE_CONTROL    = 0x02;
    // packet
    struct Packet {
        Header hd;
        char payload[maxPayloadSize];
        Packet() { }
        Packet(Header _hd) : hd(_hd) { }
        Packet(Header _hd, const char* pl) : hd(_hd) {
            memcpy(payload, pl, sizeof(char) * (hd.length - headerSize));
        }
    };

    // IPAddr
    typedef uint32_t IPAddr;
    static const IPAddr internalIPMin = 0x0a000000;
    static const IPAddr internalIPMax = 0x0affffff;
    IPAddr externalIPMin;
    IPAddr externalIPMax;
    IPAddr availableIPMin;
    IPAddr availableIPMax;
    bool isInternalIP(IPAddr ip) { return internalIPMin <= ip && ip <= internalIPMax; }
    bool isExternalIP(IPAddr ip) { return externalIPMin <= ip && ip <= externalIPMax; }
    struct availAddrAllocator {
        // 可用外网分配
        std::vector<IPAddr> address;
        availAddrAllocator() { }
        void Init(IPAddr ipMin, IPAddr ipMax) {
            address.resize(ipMax - ipMin + 1);
            for (IPAddr ip=ipMin; ip<=ipMax; ip++)
                address.push_back(ip);
        }
        IPAddr alloc() {
            if (!address.size()) return 0;
            IPAddr ret = address.back();
            address.pop_back();
            return ret;
        }
        void recycle(IPAddr ip) { address.push_back(ip); }
        bool remain() { return !address.empty(); }
    } addrAlloc;

    IPAddr IPRev(IPAddr ip) { return ((ip&0xff)<<24) | ((ip&0xff000000)>>24) | ((ip&0xff00)<<8) | ((ip&0xff0000)>>8); }
    IPAddr dotDec2IP(const char* s);
    void CIDR2IP(char *ipaddr, IPAddr& ipMin, IPAddr& ipMax);

    // host
    std::map<IPAddr, IPAddr> m_host2ExternIP; // host占据的可用外网ip
    std::map<IPAddr, IPAddr> m_externIP2Host; // host占据的可用外网ip
    std::map<IPAddr, int> m_host2Port; // host到port的映射
    static std::map<IPAddr, int> host2Router; // host到router的映射

    void releaseNATItem(IPAddr internalIP) {
        if (m_host2ExternIP.find(internalIP) == m_host2ExternIP.end()) {
            std::cerr << "internal IP not allocated an external IP\n";
            return ;
        }
        IPAddr externalIP = m_host2ExternIP[internalIP];
        addrAlloc.recycle(externalIP);
        m_externIP2Host.erase(externalIP);
        m_host2ExternIP.erase(internalIP);
    }

    // external IP
    struct EXIPMATCHER {
        int id;
        IPAddr subnetIP;
        IPAddr musk;
        bool match(IPAddr ip) { return (ip&musk) == subnetIP; }
        bool operator < (const EXIPMATCHER &rhs) {
            return musk > rhs.musk; // 最长匹配,musk更大
        }
        EXIPMATCHER() { }
        EXIPMATCHER(int _id, IPAddr _subnetIP, IPAddr _musk) : id(_id), subnetIP(_subnetIP), musk(_musk) { }
    };

    static std::vector<EXIPMATCHER> externalIPTable;
    /**
     * @brief 
     * 
     * @param exIP 外网地址
     * @return int 拥有该外网地址的路由器
     */
    int exIPMatchRouter(IPAddr exIP) {
        std::cerr << std::hex << "match IP = " << exIP << '\n';
        for(auto matcher : externalIPTable) {
            if(matcher.match(exIP)) return matcher.id;
        }
        return -1;
    }

    // struct 
    // ID
    // 路由器编号从1开始
    int m_routerID;
    static int totRouterNum ;
    struct IDManager {
        int Register(IPAddr IP, IPAddr musk) {
            std::cerr << std::hex << "register IP = " << IP << " musk = " << musk << '\n';
            totRouterNum++;
            if (!musk) return totRouterNum;
            externalIPTable.push_back(EXIPMATCHER(totRouterNum, IP, musk));
            std::sort(externalIPTable.begin(), externalIPTable.end());
            return totRouterNum;
        }
        IDManager() { }
    };
    static IDManager IDM;
    // 邻居路由器和port的相互映射
    int m_routerID2Port[MAXN]; // ID -> port
    std::map<int, int> m_port2RouterID; // port -> ID
    std::map<int, int> m_portLen; // port -> ID
    // DV
    static int lstChangeVersion; // 最新的版本，每次修改操作版本号加一
    struct DV {
        int c[MAXN];
        int d[MAXN][MAXN];
        int nextRouter[MAXN];
        int m_routerID;
        int m_version;
        void SetVersion(int versionID) { m_version = versionID; }
        DV() { }
        void Init(int mRouterID) {
            m_version = 0;
            m_routerID = mRouterID;
            memset(c, 0x3f, sizeof(c));
            memset(d, 0x3f, sizeof(d));
            memset(nextRouter, 0, sizeof(nextRouter));
            c[m_routerID] = 0;
            d[m_routerID][m_routerID] = 0;
        }
        void Reset() {
            memset(d, 0x3f, sizeof(d)); 
            memset(nextRouter, 0, sizeof(nextRouter));
            d[m_routerID][m_routerID] = 0;
        }
        
        /**
         * @brief 
         * 
         * @param tgtRID 
         * @return true changed
         * @return false unchanged
         */
        bool updateDV(int tgtRID) {
            bool changed = 0;
            for (int i=1; i<=totRouterNum; i++) {
                // 直连距离
                if (c[i] < d[m_routerID][i]) {
                    nextRouter[i] = i; // 直连
                    d[m_routerID][i] = c[i];
                    changed = 1;
                }
            }
            for (int i=1; i<=totRouterNum; i++) {
                // 跳转距离
                if (c[tgtRID] + d[tgtRID][i] < d[m_routerID][i]) {
                    nextRouter[i] = tgtRID; // 跳转
                    d[m_routerID][i] = c[tgtRID] + d[tgtRID][i];
                    changed = 1;
                }
            }

            return changed;
        }
        /**
         * @brief 
         * 
         * @param exRID  
         * @param version 
         * @return  0 没有改变不需要转发路由表
         *          1 需要转发路由表
         */
        bool updateDV(int exRID, int exDV[MAXN],  int version) {
            if (version < m_version) return 0; // 更旧的版本,忽略
            if (version > m_version) {
                // 更新版本号并且重置路由表
                m_version = version;
                Reset();
            }
            memcpy(d[exRID], exDV, sizeof(int) * MAXN);
            return updateDV(exRID);
        }
        /**
         * @brief 
         * 
         * @param tgtRID 
         * @param value 
         * @return  0 没有变化
         *          1 路径变大了，要重置所有的DV表，暂时不发路由表
         *          -1 路径变小了，不用重置，只需要迭代就行
         */
        int updateC(int tgtRID, int value) {
            value = (value == -1) ? 0x3f3f3f3f : value; // -1表示关闭
            if (c[tgtRID] == value) return 0; // 不需要更改
            if (c[tgtRID] > value) {
                // 不需要更新版本号，迭代即可
                c[tgtRID] = value;
                updateDV(tgtRID);
                return -1; 
            } else {
                
                // 需要重置所有的路由表，版本号增加
                // std::cerr << std::dec << "?????? " << m_routerID << " tgt = " << tgtRID << " value = " << value << " " << c[tgtRID] << '\n';
                SetVersion(++lstChangeVersion);
                Reset();
                c[tgtRID] = value;
                updateDV(tgtRID);
                return 1;
            }
        }
        
        int findNextRouter(int dstRouter) {
            return nextRouter[dstRouter];
        }
        /**
         * @return format: ID version DVtable
         *  
         */
        std::string toString() {
            std::stringstream ret;
            ret << m_routerID << ' ' << m_version << ' ';
            for (int i=1; i<=totRouterNum; i++)
                ret << d[m_routerID][i] << ' ';
            return ret.str();
        }
    } m_dv;
    Packet* dvInfo() {
        std::string dvString = m_dv.toString();
        Packet* ret =  new Packet(Header(0, 0, TYPE_DV, headerSize + dvString.length()), dvString.c_str());
        return ret;
    }
    
public:
    Router() { }
    void router_init(int port_num, int external_port, char* external_addr, char* available_addr);
    int router(int in_port, char* packet);

private:
    int m_port_num;
    int m_external_port;
};
