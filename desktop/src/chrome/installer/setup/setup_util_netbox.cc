#include "chrome/installer/setup/setup_util_netbox.h"

#include <shlobj.h>
#include <stddef.h>
#include <windows.h>
#include <wtsapi32.h>
#include <KnownFolders.h>

#include "base/logging.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/win/registry.h"
#include "base/win/win_util.h"

namespace installer {

void remove_wallet_registry()
{
    base::win::RegKey key_wallet;
    if (ERROR_SUCCESS == key_wallet.Open(HKEY_CURRENT_USER, L"Software", KEY_SET_VALUE)) 
    {
        key_wallet.DeleteKey(L"NetboxWallet");
    }
}

void remove_wallet_files()
{
    wchar_t system_buffer[MAX_PATH];
    system_buffer[0] = 0;

    if (FAILED(SHGetFolderPath(NULL, CSIDL_APPDATA, NULL, SHGFP_TYPE_CURRENT, system_buffer)))
    {
        VLOG(1) << "Failed to get CSIDL_APPDATA dir";
        return;
    }

    base::FilePath wallet_data_dir(system_buffer);
    wallet_data_dir = wallet_data_dir.Append(L"NetboxWallet");

    if (base::DirectoryExists(wallet_data_dir))
    {
        VLOG(1) << "Attempt to delete wallet dir: "<< wallet_data_dir.value();
        if (!base::DeletePathRecursively(wallet_data_dir)) 
        {
            VLOG(1) << "Failed to delete wallet dir: "<< wallet_data_dir.value();
        }
    }
}

}