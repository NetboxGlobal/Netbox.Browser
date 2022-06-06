#include "chrome/installer/setup/wallet_install_or_update.h"

#include "base/file_version_info.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/process/process_iterator.h"
#include "base/win/registry.h"
#include "chrome/installer/setup/wallet_stop_rpc.h"
#include "components/netboxglobal_utils/utils.h"
#include "components/netboxglobal_utils/wallet_utils.h"

static const std::vector<base::Version> wallet_check_versions =
{
    base::Version("3.3.0.3"),
    base::Version("3.3.0.2"),
    base::Version("3.3.0.1"),
    base::Version("3.3.0.0"),
};

static bool get_file_version(const std::wstring& path, base::Version &result)
{
    DWORD ver_handle = 0;
    DWORD ver_size = GetFileVersionInfoSize(path.c_str(), &ver_handle);
    if (0 == ver_size)
    {
        return false;
    }

    std::unique_ptr<char> ver_buf(new char[ver_size]);

    if (TRUE == GetFileVersionInfo(path.c_str(), ver_handle, ver_size, ver_buf.get()))
    {
        VS_FIXEDFILEINFO* ver_info = NULL;
        unsigned int info_size = 0;
        if (TRUE == VerQueryValue(ver_buf.get(), L"\\", reinterpret_cast<LPVOID*>(&ver_info), &info_size))
        {
            if (0 != info_size)
            {
                std::vector<uint32_t> components = {(ver_info->dwFileVersionMS >> 16) & 0xff,
                                                     ver_info->dwFileVersionMS        & 0xff,
                                                    (ver_info->dwFileVersionLS >> 16) & 0xff,
                                                     ver_info->dwFileVersionLS        & 0xff};
                result = base::Version(components);
            }
            return 0 != info_size;
        }
    }

    return false;
}

static void delete_wallet_registry_data()
{
    base::win::RegKey key;

    LONG result = key.Open(HKEY_CURRENT_USER, L"Software\\NetboxWallet\\NetboxWallet-Qt", KEY_ALL_ACCESS);

    if (ERROR_SUCCESS == result)
    {
        result = key.DeleteValue(L"current_receive_address");
        if (ERROR_SUCCESS != result)
        {
            LOG(ERROR) << "delete_wallet_reg_data, failed to delete 'current_receive_address', " << result;
        }

        result = key.DeleteValue(L"fUseUPnP");
        if (ERROR_SUCCESS != result)
        {
            LOG(ERROR) << "delete_wallet_reg_data, failed to delete 'fUseUPnP' value, " << result;
        }

        VLOG(1) << "delete_wallet_reg_data, success";
    }
    else
    {
        LOG(ERROR) << "delete_wallet_reg_data, failed to open 'Software\\NetboxWallet\\NetboxWallet-Qt', " << result;
    }
}

static bool send_stop_signal_to_wallet_and_wait(const std::string& log_mode)
{
    if (0 == base::GetProcessCount(Netboxglobal::get_wallet_filename(), nullptr))
    {
        VLOG(1) << log_mode << ", no wallet process";
        return true;
    }

    VLOG(1) << log_mode << ", sending stop signal";

    DWORD rpc_http_status = 0;
    DWORD res = WalletStopRPC::send(rpc_http_status);

    bool rpc_sended = false;
    if (S_OK != res)
    {
        VLOG(1) << log_mode << ", failed to send RPC stop signal " << res;
    }
    else
    {
        if (200 == rpc_http_status || res == 12031)
        {
            rpc_sended = true;
        }
        else
        {
            VLOG(1) << log_mode << ", stop signal http error " << rpc_http_status;
        }
    }

    // if we didn't reach wallet by RPC
    if (!rpc_sended)
    {
        base::NamedProcessIterator process_it(Netboxglobal::get_wallet_filename(), nullptr);
        while (const auto* entry = process_it.NextProcessEntry())
        {
            Netboxglobal::send_hwnd_stop_signal_to_wallet(entry->pid());
        }
    }

    // wait 3 minutes for wallet
    base::NamedProcessIterator process_it(Netboxglobal::get_wallet_filename(), nullptr);
    while (const auto* entry = process_it.NextProcessEntry())
    {
        base::Process process = base::Process::Open(entry->pid());

        if (!process.IsValid())
        {
            continue;
        }

        process.WaitForExitWithTimeout(base::TimeDelta::FromMinutes(3), nullptr);
    }

    return true;
}

void wallet_install_or_update(const base::FilePath& installer_wallet_folder)
{
    VLOG(1) << L"checking wallet version";

    base::FilePath wallet_current_path;

    bool res = Netboxglobal::get_wallet_exe_path(&wallet_current_path, FILE_PATH_LITERAL(""));

    VLOG(1) << L"wallet path is " << wallet_current_path.value();

    base::Version wallet_current_version;
    bool version_found = false;
    if (res)
    {
        version_found = get_file_version(wallet_current_path.value(), wallet_current_version);
    }

    if (true == version_found)
    {
        VLOG(1) << L"wallet version, " << wallet_current_version;
    }
    else
    {
        VLOG(1) << L"wallet version, not found";
    }

    VLOG(0) << L"wallet folder:" << installer_wallet_folder;

    base::FilePath wallet_cur_path;
    if (!Netboxglobal::get_wallet_exe_path(&wallet_cur_path, FILE_PATH_LITERAL("")))
    {
        VLOG(1) << L"can't get current wallet path";
        return;
    }

    base::FilePath wallet_old_path;
    if (!Netboxglobal::get_wallet_exe_path(&wallet_old_path, FILE_PATH_LITERAL("tmp")))
    {
        VLOG(1) << L"can't get oldwallet path";
        return;
    }

    // prepare
    base::FilePath wallet_installation_path = installer_wallet_folder.Append(Netboxglobal::get_wallet_filename());
    if (!base::PathExists(wallet_installation_path))
    {
        VLOG(1) << L"netboxwallet didn't exist in installation" << wallet_installation_path;
        return;
    }

    // remove previous old files
    if (base::PathExists(wallet_old_path))
    {
        base::DeleteFile(wallet_old_path);
    }

    bool wallet_exists = base::PathExists(wallet_cur_path);

    if (wallet_exists)
    {
        VLOG(1) << "current netbox wallet exists";

        if (!base::Move(wallet_cur_path, wallet_old_path))
        {
            VLOG(1) << L"can't move " << wallet_cur_path << L" to " << wallet_old_path;
        }

        send_stop_signal_to_wallet_and_wait("install_or_update");
    }

    // correct registry
    for (const auto &v : wallet_check_versions)
    {
        if (wallet_current_version == v)
        {
            delete_wallet_registry_data();
            break;
        }
    }

    base::FilePath wallet_cur_path_new = installer_wallet_folder.Append(Netboxglobal::get_wallet_filename() + FILE_PATH_LITERAL(".new"));

    // copy to correct place from installation
    base::CopyFile(wallet_installation_path, wallet_cur_path_new);
    base::ReplaceFile(wallet_cur_path_new, wallet_cur_path, nullptr);

    if (base::PathExists(wallet_old_path))
    {
        if (false == base::DeleteFile(wallet_old_path))
        {
            VLOG(1) << "cant delete temp netbox wallet";
        }
        else
        {
            VLOG(1) << "temp netbox wallet deleted";
        }
    }
}

void wallet_install_or_update(const base::FilePath& target_path, const std::unique_ptr<base::Version> &installer_version)
{
    if (installer_version.get())
    {
        base::FilePath installer_wallet_folder = target_path.Append(base::UTF8ToWide(installer_version->GetString())).Append(FILE_PATH_LITERAL("wallet"));

        wallet_install_or_update(installer_wallet_folder);
    }
    else
    {
        VLOG(1) << L"can't get version";
    }
}