#ifndef COMPONENTS_NETBOXGLOBAL_CALL_WALLET_TAB_HANDLER_H_
#define COMPONENTS_NETBOXGLOBAL_CALL_WALLET_TAB_HANDLER_H_

#include <string>

#include "base/macros.h"
#include "base/values.h"

namespace Netboxglobal
{

class IWalletTabHandler
{
public:
    virtual void OnTabCall(std::string event_name, base::Value result) = 0;
};

}

#endif