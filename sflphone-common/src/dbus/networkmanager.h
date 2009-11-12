#ifndef NETWORKMANAGER_H
#define NETWORKMANAGER_H

#include "networkmanager_proxy.h"

using namespace std;

class NetworkManager
: public org::freedesktop::NetworkManager_proxy,
  public DBus::IntrospectableProxy,
  public DBus::ObjectProxy
{
public:

    NetworkManager(DBus::Connection&, const DBus::Path&, const char*);
    void StateChanged(const uint32_t& state);
    string stateAsString(const uint32_t& state);

    typedef enum NMState
    {
        NM_STATE_UNKNOWN = 0,
        NM_STATE_ASLEEP,
        NM_STATE_CONNECTING,
        NM_STATE_CONNECTED,
        NM_STATE_DISCONNECTED
    } NMState;

   static const string statesString[5];
};
#endif

