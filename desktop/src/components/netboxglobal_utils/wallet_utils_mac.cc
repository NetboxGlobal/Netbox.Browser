#include "components/netboxglobal_utils/wallet_utils.h"

#include "base/logging.h"
#include "base/path_service.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/chrome_constants.h"
#include "components/netboxglobal_utils/utils.h"

namespace Netboxglobal
{

base::FilePath get_wallet_data_folder()
{
    base::FilePath user_data_dir;
    base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);

    base::FilePath path = user_data_dir.DirName().Append("NetboxWallet");

    if (true == is_qa())
    {
        path = path.Append(FILE_PATH_LITERAL("testnet4"));
    }

    return path;
}

bool get_wallet_exe_path(base::FilePath* path, const base::FilePath::StringType&)
{
    if (path == nullptr)
    {
        return false;
    }

    *path = chrome::GetFrameworkBundlePath().Append("Resources").Append(get_wallet_filename());

    return true;
}

}