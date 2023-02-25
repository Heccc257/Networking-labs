#include "switch.h"
#include <vector>

using namespace std;

SwitchBase* CreateSwitchObject() {
  	// TODO : Your code.
	return new EthernetSwitch();
}

void EthernetSwitch::InitSwitch(int numPorts) {
	table.clear();
}

void EthernetSwitch::Aging() {
	vector<long long>del;
	for (auto &it : table) {
		it.second.lifeTime--;
		if (it.second.lifeTime == 0) del.push_back(it.first);
	}
	for (auto a : del) table.erase(table.find(a));
}

int EthernetSwitch::ProcessFrame(int inPort, char* framePtr) {
	ether_header_t* header = (ether_header_t*)((void*)framePtr);
	if (header->ether_type == ETHER_CONTROL_TYPE) {
		Aging();
		return -1;
	} else {
		long long src = macAddr2Long(header->ether_src);
		long long dest = macAddr2Long(header->ether_dest);

		table[src].lifeTime = ETHER_MAC_AGING_THRESHOLD;
		table[src].Port = inPort;

		auto destIt = table.find(dest);
		if (destIt == table.end()) { // 没找到
			return 0;
		} else {
			auto destEntry = destIt->second;
			if (destEntry.Port == inPort) return -1;
			return destEntry.Port;
		}
	}

}