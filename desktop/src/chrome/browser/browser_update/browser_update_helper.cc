#include "chrome/browser/browser_update/browser_update_helper.h"

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/memory/singleton.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/process/process_iterator.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/chrome_version.h"
#include "components/netboxglobal_utils/wallet_utils.h"
#include "content/public/common/content_switches.h"

BrowserUpdateMacHelper::BrowserUpdateMacHelper(){}

BrowserUpdateMacHelper::~BrowserUpdateMacHelper(){}

void BrowserUpdateMacHelper::set_dmg_path(base::FilePath path)
{
    dmg_path_ = path;
}

bool BrowserUpdateMacHelper::is_set_dmg_path()
{
    return base::PathExists(dmg_path_);
}

void BrowserUpdateMacHelper::clear()
{
    if (base::PathExists(dmg_path_))
    {
        base::DeleteFile(dmg_path_);
    }
}

void BrowserUpdateMacHelper::launch_updater()
{
    base::FilePath updater_path = chrome::GetFrameworkBundlePath().Append("Resources").Append("netbox_updater.sh");

    base::FilePath app_path;
    base::PathService::Get(base::FILE_EXE, &app_path);
    app_path = app_path.DirName().DirName().DirName();

    base::CommandLine cmdline(updater_path);

    // parameter 1 path to current exe
    cmdline.AppendArgPath(app_path);

    // parameter 2 current version
    cmdline.AppendArg(CHROME_VERSION_STRING);

    // parameter 3 path to dmg
    cmdline.AppendArgPath(dmg_path_);

    // parameter 4 current pid
    base::Process current = base::Process::Current();
    cmdline.AppendArg(std::to_string(current.Pid()));

    // parameter 5 wallet pid
    std::string wallet_pid = "0";
    base::NamedProcessIterator process_it(Netboxglobal::get_wallet_filename(), nullptr);
    while (const auto* entry = process_it.NextProcessEntry())
    {
        wallet_pid = std::to_string(entry->pid());
    }
    cmdline.AppendArg(wallet_pid);

    // parameter 6 logging
    const base::CommandLine* browser_cmdline = base::CommandLine::ForCurrentProcess();
    if (browser_cmdline && browser_cmdline->HasSwitch(switches::kEnableLogging))
    {
        cmdline.AppendArg("enable-logging");
    }

    VLOG(NETBOX_LOG_LEVEL) << "update helper process, run, start, " << cmdline.GetCommandLineString();
    base::Process process = base::LaunchProcess(cmdline, base::LaunchOptions());
    if (!process.IsValid())
    {
        VLOG(NETBOX_LOG_LEVEL) << "update helper process, run, start error: " << cmdline.GetCommandLineString();
    }
}

BrowserUpdateMacHelper* BrowserUpdateMacHelper::GetInstance()
{
    return base::Singleton<BrowserUpdateMacHelper>::get();
}
