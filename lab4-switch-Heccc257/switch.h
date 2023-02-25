#ifndef COMPNET_LAB4_SRC_SWITCH_H
#define COMPNET_LAB4_SRC_SWITCH_H

#include "types.h"
#include <map>

class SwitchBase {
 public:
  SwitchBase() = default;
  ~SwitchBase() = default;

  virtual void InitSwitch(int numPorts) = 0;
  virtual int ProcessFrame(int inPort, char* framePtr) = 0;
};

extern SwitchBase* CreateSwitchObject();

// TODO : Implement your switch class.
class EthernetSwitch : public SwitchBase {
public:
  struct Entry {
    int Port;
    int lifeTime;
    Entry() { }
    Entry(int _port, int _lifetime) : Port(_port), lifeTime(_lifetime) { }
  };
private:
  std::map<long long, Entry> table;

public:
  void InitSwitch(int numPorts);
  int ProcessFrame(int inPort, char* framePtr);
  void Aging();
};

#endif  // ! COMPNET_LAB4_SRC_SWITCH_H
