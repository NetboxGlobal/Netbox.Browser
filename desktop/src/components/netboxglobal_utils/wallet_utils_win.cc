#include "components/netboxglobal_utils/wallet_utils.h"

#include <regex>
#include <string>
#include <memory>
#include <functional>
#include <windows.h>
#include <winioctl.h>
#include <Shlobj.h>
#include <tlhelp32.h>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/win/wmi.h"
#include "base/win/scoped_variant.h"
#include "chrome/install_static/install_modes.h"
#include "components/netboxglobal_utils/utils.h"
#include "components/netboxglobal_utils/wallet_utils.h"

namespace Netboxglobal
{

base::FilePath get_wallet_data_folder()
{
    base::FilePath path;
    if (!base::PathService::Get(base::DIR_ROAMING_APP_DATA, &path))
    {
        VLOG(NETBOX_LOG_LEVEL) << L"can't get roaming appdata";
        return base::FilePath();
    }

    path = path.Append(FILE_PATH_LITERAL("NetboxWallet"));

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
    if (!base::PathService::Get(base::DIR_LOCAL_APP_DATA, &tmp))
    {
        VLOG(NETBOX_LOG_LEVEL) << L"can't get local appdata";
        return false;
    }

    *path = tmp.Append(install_static::kProductPathName).Append(prefix + get_wallet_filename());

    return true;
}

}