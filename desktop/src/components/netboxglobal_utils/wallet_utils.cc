#include "components/netboxglobal_utils/wallet_utils.h"

#include "base/path_service.h"
#include "base/files/file_util.h"

namespace Netboxglobal
{


base::FilePath::StringType get_wallet_filename()
{
    #if defined(OS_WIN)
        return L"netboxwallet.exe";
    #elif defined(OS_MAC)
        return "NetboxWallet.app";
    #else
        return "netboxwallet";
    #endif
}

base::FilePath get_token_path()
{
    return get_wallet_data_folder().Append(FILE_PATH_LITERAL(".cookie"));
}

std::string read_token_raw()
{
    std::string token;
    if (ReadFileToStringWithMaxSize(get_token_path(), &token, 100u))
    {
        return token;
    }

    return "";
}


}