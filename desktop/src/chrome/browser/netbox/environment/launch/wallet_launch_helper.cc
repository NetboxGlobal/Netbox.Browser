#include "chrome/browser/netbox/environment/launch/wallet_launch_helper.h"

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/i18n/time_formatting.h"
#include "base/logging.h"
#include "base/process/launch.h"
#include "base/process/process_iterator.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/netbox/environment/controller/wallet_environment.h"
#include "components/netboxglobal_utils/utils.h"
#include "components/netboxglobal_utils/wallet_utils.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace Netboxglobal
{

static const int32_t WALLET_FILE_TIMEOUT_MS = 1000;
static const int32_t WALLET_FILE_TRIES_CNT  = 10; // TOTAL WAIT - 10 SECONDS

static const int32_t DELETE_TOKEN_TIMEOUT_MS = 500;
static const int32_t DELETE_TOKEN_TRIES_CNT = 10; // TOTAL WAIT - 5 SECONDS

static const int32_t WAIT_PROCESS_TIMEOUT_MS = 1000;
static const int32_t WAIT_PROCESS_TRIES_CNT = 1800; // TOTAL WAIT - 30 MINUTES

static const int32_t TOKEN_FILE_READ_TIMEOUT_MS = 1000;
static const int32_t TOKEN_FILE_READ_TRIES_CNT  = 180; // TOTAL WAIT - 3 MINUTES

base::CommandLine get_cmd(const base::FilePath &wallet_path)
{
    base::CommandLine cmd(wallet_path);
    cmd.AppendSwitch("server");
    cmd.AppendSwitchASCII("splash", "0");
    cmd.AppendSwitchASCII("lang", "en");
    cmd.AppendSwitchASCII("staking", "0");
    cmd.AppendSwitch("dappstore");

    cmd.AppendSwitch("hide");

    if (true == is_qa())
    {
        cmd.AppendSwitch("testnet");
    }

    return cmd;
}

base::TaskTraits get_default_traits()
{
    return {
        base::MayBlock(),
        base::WithBaseSyncPrimitives(),
        base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN
    };

}

void wallet_launch_result(WalletSessionManager::DataState state, std::string token = "")
{
	VLOG(NETBOX_LOG_LEVEL) << L"wallet_launch, result: " << state;

    base::FilePath file_path;
    get_wallet_exe_path(&file_path, {});

    std::string wallet_exe_creation_time;
    base::File::Info file_info;
    if (base::GetFileInfo(file_path, &file_info))
    {
        wallet_exe_creation_time = base::UTF16ToASCII(base::TimeFormatShortDateAndTime(file_info.creation_time));
    }

    // UITHREAD
    base::PostTask(
        FROM_HERE,
        {
            content::BrowserThread::UI,
            base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN
        },
        base::BindOnce(&WalletSessionManager::on_wallet_start, base::Unretained(g_browser_process->env_controller()), state, std::move(token), std::move(wallet_exe_creation_time))
    );
}

void stop_old_unix_processes()
{
    #if defined(OS_LINUX)
    base::NamedProcessIterator process_it(get_wallet_filename() + " (deleted)", nullptr);
    while (const auto* entry = process_it.NextProcessEntry())
    {
        base::Process process = base::Process::Open(entry->pid());
        if (process.IsValid())
        {
			VLOG(NETBOX_LOG_LEVEL) << L"wallet_launch, stop old process";
            process.Terminate(0, true);
        }
    }
    #endif
}

void wait_process(WALLET_LAUNCH mode, int wait_iteration);
void wallet_launch_raw(WALLET_LAUNCH mode, int launch_iteration);
void delete_old_token(WALLET_LAUNCH mode, int delete_token_iteration);
void read_token(int read_token_iteration);

void stop_previous_processes(WALLET_LAUNCH mode)
{
	VLOG(NETBOX_LOG_LEVEL) << L"wallet_launch, stop process";

    base::NamedProcessIterator process_it(get_wallet_filename(), nullptr);
    while (const auto* entry = process_it.NextProcessEntry())
    {
        base::Process process = base::Process::Open(entry->pid());

        if (!process.IsValid())
        {
            VLOG(1) << "process is not valid";
            continue;
        }

		#if defined(OS_WIN)
			send_hwnd_stop_signal_to_wallet(entry->pid());
		#else
			process.Terminate(0, true);
		#endif

		base::ThreadPool::PostTask(
			FROM_HERE,
            get_default_traits(),
			base::BindOnce(&wait_process, mode, 0));

		return;
	}

	base::ThreadPool::PostTask(
		FROM_HERE,
        get_default_traits(),
		base::BindOnce(&wallet_launch_raw, mode, 0));
}

void wait_process(WALLET_LAUNCH mode, int wait_iteration)
{
	VLOG(NETBOX_LOG_LEVEL) << "wallet_launch, wait process";

    base::NamedProcessIterator process_it(get_wallet_filename(), nullptr);
    while (const auto* entry = process_it.NextProcessEntry())
    {
        base::Process process = base::Process::Open(entry->pid());

        if (!process.IsValid())
        {
            VLOG(1) << "process is not valid";
            continue;
        }

		if (wait_iteration > WAIT_PROCESS_TRIES_CNT)
		{
			VLOG(NETBOX_LOG_LEVEL) << L"wallet_launch, wait process failed";
			return wallet_launch_result(WalletSessionManager::DS_WALLET_PROCESS_STOPPED_WITH_ERROR);
		}

		wait_iteration++;

        base::ThreadPool::PostDelayedTask(
            FROM_HERE,
            get_default_traits(),
            base::BindOnce(&wait_process, mode, wait_iteration),
            base::TimeDelta::FromMilliseconds(WAIT_PROCESS_TIMEOUT_MS));

		return;
	}

	// START PROCESS
	base::ThreadPool::PostTask(
		FROM_HERE,
        get_default_traits(),
		base::BindOnce(&delete_old_token, mode, 0));
}

#if defined(OS_WIN)
bool check_after_start()
{
	VLOG(NETBOX_LOG_LEVEL) << "wallet_launch, check after start";

    // CHECK AFTER START
    base::NamedProcessIterator process_it(get_wallet_filename(), nullptr);
    while (const auto* entry = process_it.NextProcessEntry())
    {
        // TODO: MinGW Runtime Assertion
        WindowsSearchParams params = {entry->pid(), L"Qt5QWindowIcon", L"Netbox.Wallet - Error", false, false};
        find_window(params);

        if (false == params.result.empty())
        {
            VLOG(NETBOX_LOG_LEVEL) << L"wallet_launch, wallet start error";
            return false;
        }
    }

    return true;
}
#endif

void delete_old_token(WALLET_LAUNCH mode, int delete_token_iteration)
{
	base::FilePath file_path = get_token_path();
	if (base::PathExists(file_path))
	{
		VLOG(NETBOX_LOG_LEVEL) << "wallet_launch, old token deleted";
		base::DeleteFile(file_path);
	}

	if (base::PathExists(file_path) && delete_token_iteration <= DELETE_TOKEN_TRIES_CNT)
	{
		VLOG(NETBOX_LOG_LEVEL) << "wallet_launch, old token still exists";

		delete_token_iteration++;

		base::ThreadPool::PostDelayedTask(
			FROM_HERE,
            get_default_traits(),
			base::BindOnce(&delete_old_token, mode, delete_token_iteration),
			base::TimeDelta::FromMilliseconds(DELETE_TOKEN_TIMEOUT_MS));
	}

	base::ThreadPool::PostTask(
	    FROM_HERE,
        get_default_traits(),
	    base::BindOnce(&wallet_launch_raw, mode, 0));
}

void wallet_launch_raw(WALLET_LAUNCH mode, int launch_iteration)
{
	VLOG(NETBOX_LOG_LEVEL) << "wallet_launch, launch raw";

    base::FilePath path;
    if (!get_wallet_exe_path(&path, {}))
    {

        VLOG(NETBOX_LOG_LEVEL) << L"wallet_launch, launch raw, failed to get wallet path";
        return wallet_launch_result(WalletSessionManager::DS_WALLET_DIR_NOT_FOUND);
    }

    VLOG(NETBOX_LOG_LEVEL) << "wallet_launch, launch raw, " << path;

    if (false == base::PathExists(path))
    {
        if (launch_iteration > WALLET_FILE_TRIES_CNT)
        {
            VLOG(NETBOX_LOG_LEVEL) << "wallet_launch, launch raw, wallet not found";
            return wallet_launch_result(WalletSessionManager::DS_WALLET_FILE_NOT_FOUND);
        }
        launch_iteration++;

        base::ThreadPool::PostDelayedTask(
            FROM_HERE,
            get_default_traits(),
            base::BindOnce(&wallet_launch_raw, mode, launch_iteration),
            base::TimeDelta::FromMilliseconds(WALLET_FILE_TIMEOUT_MS));

        return;
    }

    if (0 == base::GetProcessCount(get_wallet_filename(), nullptr))
    {
        base::CommandLine cmd = get_cmd(path);        
        base::Process wallet_handle = base::LaunchProcess(cmd, base::LaunchOptions());

        if (!wallet_handle.IsValid())
        {
            VLOG(NETBOX_LOG_LEVEL) << L"wallet_launch, start failed, " << cmd.GetCommandLineString();
            return wallet_launch_result(WalletSessionManager::DS_WALLET_PROCESS_NOT_FOUND);
        }

        VLOG(NETBOX_LOG_LEVEL) << "wallet_launch, start success, " << cmd.GetCommandLineString();
    }


    #if defined(OS_WIN)
    if (false == check_after_start())
    {
        return wallet_launch_result(WalletSessionManager::DS_WALLET_PROCESS_STARTED_WITH_ERROR);
    }
    #endif

    base::ThreadPool::PostTask(
        FROM_HERE,
        get_default_traits(),
        base::BindOnce(&read_token, 0));
}

void read_token(int read_token_iteration)
{
    std::string token = read_token_raw();
    if (!token.empty())
    {
        VLOG(NETBOX_LOG_LEVEL) << L"wallet_launch, token success";
        return wallet_launch_result(WalletSessionManager::DS_OK, token);
    }

    VLOG(NETBOX_LOG_LEVEL) << "wallet_launch, token fail";

    if (read_token_iteration > TOKEN_FILE_READ_TRIES_CNT)
    {
        VLOG(NETBOX_LOG_LEVEL) << L"wallet_launch, token failed, limit";
        return wallet_launch_result(WalletSessionManager::DS_WALLET_RPC_TOKEN_ERROR);
    }

    VLOG(NETBOX_LOG_LEVEL) << L"wallet_launch, token reread";
    read_token_iteration++;

    base::ThreadPool::PostDelayedTask(
        FROM_HERE,
        get_default_traits(),
        base::BindOnce(&read_token, read_token_iteration),
        base::TimeDelta::FromMilliseconds(TOKEN_FILE_READ_TIMEOUT_MS));
}

void wallet_launch(WALLET_LAUNCH mode)
{
    stop_old_unix_processes();

	switch(mode)
	{
		case STOP_AND_START:
		{
			base::ThreadPool::PostTask(
				FROM_HERE,
                get_default_traits(),
				base::BindOnce(&stop_previous_processes, mode));
			return;
		}
		case WAIT_AND_START:
		{
			base::ThreadPool::PostTask(
				FROM_HERE,
                get_default_traits(),
				base::BindOnce(&wait_process, mode, 0));
			return;
		}
		default:
		{
			// START PROCESS
			base::ThreadPool::PostTask(
				FROM_HERE,
                get_default_traits(),
				base::BindOnce(&wallet_launch_raw, mode, 0));
			return;
		}
	}
}

}
