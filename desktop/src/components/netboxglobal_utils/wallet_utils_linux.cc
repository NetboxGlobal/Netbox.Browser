#include "components/netboxglobal_utils/wallet_utils.h"

#include "base/logging.h"
#include "base/path_service.h"
#include "chrome/common/chrome_paths.h"
#include "components/netboxglobal_utils/utils.h"

namespace Netboxglobal
{

base::FilePath get_wallet_data_folder()
{
    base::FilePath path;
    base::PathService::Get(base::DIR_HOME, &path);

    path = path.Append(FILE_PATH_LITERAL(".NetboxWallet"));

    if (true == is_qa())
    {
        path = path.Append(FILE_PATH_LITERAL("testnet4"));
    }

    return path;
}

bool get_wallet_exe_path(base::FilePath* path, const base::FilePath::StringType &prefix)
{
    if (path == nullptr)
    {
        return false;
    }

    base::FilePath tmp;
    if (!base::PathService::Get(base::DIR_EXE, &tmp))
    {
        VLOG(NETBOX_LOG_LEVEL) << L"failed to get path for wallet executable";
        return false;
    }

    *path = tmp.Append(prefix + get_wallet_filename());

    return true;
}


}
