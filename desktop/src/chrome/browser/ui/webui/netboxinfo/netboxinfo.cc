#include "chrome/browser/ui/webui/netboxinfo/netboxinfo.h"

#include "chrome/browser/browser_process.h"
#include "base/cpu.h"
#include "base/files/file_util.h"
#include "base/memory/singleton.h"
#include "base/memory/ref_counted_memory.h"
#include "base/path_service.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/thread.h"
#include "base/values.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/metrics_handler.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/netbox_resources.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "chrome/browser/netbox/environment/controller/wallet_environment.h"
#include "components/netboxglobal_utils/utils.h"
#include "components/netboxglobal_utils/wallet_utils.h"
#include "components/prefs/pref_service.h"
#include "components/storage_monitor/storage_info.h"
#include "components/storage_monitor/storage_monitor.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/content_features.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/text/bytes_formatting.h"

#if defined(OS_WIN)
#include "chrome/installer/util/install_util.h"
#endif

using content::BrowserContext;
using content::DownloadManager;
using content::WebContents;

static base::FilePath GetBrowserLogPath()
{
    base::FilePath browser_log_dir;
    base::PathService::Get(chrome::DIR_LOGS, &browser_log_dir);

    return browser_log_dir.Append(FILE_PATH_LITERAL("netbox_debug.log"));
}

static base::FilePath GetWalletLogPath()
{
    base::FilePath wallet_working_dir = Netboxglobal::get_wallet_data_folder();
    return wallet_working_dir.Append(FILE_PATH_LITERAL("debug.log"));
}

static void GetFilePaths(const base::FilePath& profile_path,
                         std::u16string* exec_path_out,
                         std::u16string* profile_path_out)
{
    base::ScopedBlockingCall scoped_blocking_call(FROM_HERE, base::BlockingType::MAY_BLOCK);

    base::FilePath executable_path = base::MakeAbsoluteFilePath(base::CommandLine::ForCurrentProcess()->GetProgram());
    base::FilePath profile_path_copy = base::MakeAbsoluteFilePath(profile_path);

    *exec_path_out = (false == executable_path.empty()) ? executable_path.LossyDisplayName() : u"Path not found";
    *profile_path_out = (false == profile_path_copy.empty() && false == profile_path.empty()) ? executable_path.LossyDisplayName() : u"Path not found";
}

static void CheckWalletPaths(bool *wallet_exe_exists, bool *wallet_temp_exe_exists, bool *wallet_instaler_exe_exists)
{
    base::ScopedBlockingCall scoped_blocking_call(FROM_HERE, base::BlockingType::MAY_BLOCK);

    base::FilePath exe_path;
    Netboxglobal::get_wallet_exe_path(&exe_path, FILE_PATH_LITERAL(""));

    base::FilePath exe_tmp_path;
    Netboxglobal::get_wallet_exe_path(&exe_tmp_path, FILE_PATH_LITERAL("tmp"));

    *wallet_instaler_exe_exists = false;
#if defined(OS_WIN)
    base::FilePath installer_path;
    if (true == base::PathService::Get(base::DIR_LOCAL_APP_DATA, &installer_path))
    {
        base::Version version = InstallUtil::GetChromeVersion(false);

        if (true == version.IsValid())
        {
            installer_path = installer_path.Append(FILE_PATH_LITERAL("NetboxBrowser"));
            installer_path = installer_path.Append(FILE_PATH_LITERAL("Application"));
            installer_path = installer_path.Append(base::UTF8ToWide(version.GetString()));
            installer_path = installer_path.Append(FILE_PATH_LITERAL("wallet"));
            installer_path = installer_path.Append(FILE_PATH_LITERAL("netboxwallet.exe"));

            *wallet_instaler_exe_exists = base::PathExists(installer_path);
        }
    }
#endif
    *wallet_exe_exists = base::PathExists(exe_path);
    *wallet_temp_exe_exists = base::PathExists(exe_tmp_path);
}

static void GetDisksFreeSpace(std::vector<std::pair<std::wstring, int64_t>> *data)
{
    base::ScopedBlockingCall scoped_blocking_call(FROM_HERE, base::BlockingType::MAY_BLOCK);
//NETBOXTODO UNIX port
#if defined(OS_WIN)
    const size_t BUFSIZE = 512;

    TCHAR data_string[BUFSIZE] = {};

    TCHAR* p = data_string;

    if (0 != GetLogicalDriveStrings(BUFSIZE-1, data_string))
    {
        while('\0' != *p)
        {
            std::wstring name = p;

            data->push_back({name, base::SysInfo::AmountOfFreeDiskSpace(base::FilePath(name))});

            p += wcslen(p) + 1;
        }
    }
#endif
}

static void GetOperatingSystemType(bool *is_64bit)
{
    *is_64bit = Netboxglobal::is_system_64bit();
}


class MessageHandler : public content::WebUIMessageHandler
{
public:
    MessageHandler();
    ~MessageHandler() override;

    void RegisterMessages() override;

 private:

    void HandleGetInfo(const base::ListValue *);
    void HandleOpenFile(const base::ListValue *);

    void OnGotDisksFreeSpace(std::vector<std::pair<std::wstring, int64_t>> *data);
    void OnGotFilePaths(std::u16string* executable_path_data, std::u16string* profile_path_data);
    void OnGotOperatingSystemType(bool *is_64);
    void OnCheckWalletPaths(bool *wallet_exe_exists, bool *wallet_temp_exe_exists, bool *wallet_instaler_exe_exists);

    base::WeakPtrFactory<MessageHandler> weak_ptr_factory_{this};

    DISALLOW_COPY_AND_ASSIGN(MessageHandler);
};

MessageHandler::MessageHandler()
{}

MessageHandler::~MessageHandler()
{}

void MessageHandler::RegisterMessages()
{
    web_ui()->RegisterMessageCallback("netboxinfo",             base::BindRepeating(&MessageHandler::HandleGetInfo, weak_ptr_factory_.GetWeakPtr()));
    web_ui()->RegisterMessageCallback("openfile",               base::BindRepeating(&MessageHandler::HandleOpenFile, weak_ptr_factory_.GetWeakPtr()));
}

void MessageHandler::HandleGetInfo(const base::ListValue *)
{
    std::string bitCntStr = sizeof(void*) == 8 ? "(64 bit)" : "(32 bit)";

    base::DictionaryValue data;
    data.SetKey("Browser log",              base::Value(GetBrowserLogPath().LossyDisplayName()));
    data.SetKey("Browser version",          base::Value(std::string(CHROME_VERSION_STRING) + " " +  bitCntStr));
    #if defined(OS_WIN)
    data.SetKey("Command line",             base::Value(base::WideToUTF16(base::CommandLine::ForCurrentProcess()->GetCommandLineString())));
    #else
    data.SetKey("Command line",             base::Value(base::CommandLine::ForCurrentProcess()->GetCommandLineString()));
    #endif
    data.SetKey("CPU name",                 base::Value(base::CPU().cpu_brand()));
    data.SetKey("Operating System Name",    base::Value(base::SysInfo::OperatingSystemName()));
    data.SetKey("Operating System Version", base::Value(base::SysInfo::OperatingSystemVersion()));
    data.SetKey("Physical memory (MB)",     base::Value(base::SysInfo::AmountOfPhysicalMemoryMB()));
    data.SetKey("Wallet log",               base::Value(GetWalletLogPath().LossyDisplayName()));
    data.SetKey("Wallet exe creation time", base::Value(g_browser_process->env_controller()->get_wallet_exe_creation_time()));

    data.SetKey("Debug information",        base::Value(g_browser_process->env_controller()->get_encrypted_machine_id()));

    if (true == g_browser_process->env_controller()->is_qa())
	{
        data.SetKey("Is testnet", base::Value(true));
	}

	base::Value event_name("netboxinfo.common");
	web_ui()->CallJavascriptFunctionUnsafe("wallet_send_event", std::move(event_name), std::move(data));

    std::u16string* exec_path_buffer = new std::u16string();
    std::u16string* profile_path_buffer = new std::u16string();

    base::TaskTraits blocking_task_traits = {
            base::TaskPriority::USER_VISIBLE,
            base::MayBlock()
    };

    base::ThreadPool::PostTaskAndReply(
        FROM_HERE,
        blocking_task_traits,
        base::BindOnce(&GetFilePaths, Profile::FromWebUI(web_ui())->GetPath(), base::Unretained(exec_path_buffer),base::Unretained(profile_path_buffer)),
        base::BindOnce(&MessageHandler::OnGotFilePaths, weak_ptr_factory_.GetWeakPtr(), base::Owned(exec_path_buffer), base::Owned(profile_path_buffer)));

    std::vector<std::pair<std::wstring, int64_t>> *disks_data = new std::vector<std::pair<std::wstring, int64_t>>();

    base::ThreadPool::PostTaskAndReply(
        FROM_HERE,
        blocking_task_traits,
        base::BindOnce(&GetDisksFreeSpace, base::Unretained(disks_data)),
        base::BindOnce(&MessageHandler::OnGotDisksFreeSpace, weak_ptr_factory_.GetWeakPtr(), base::Owned(disks_data)));

    bool *is_64bit = new bool();
    base::ThreadPool::PostTaskAndReply(
        FROM_HERE,
        blocking_task_traits,
        base::BindOnce(&GetOperatingSystemType, base::Unretained(is_64bit)),
        base::BindOnce(&MessageHandler::OnGotOperatingSystemType, weak_ptr_factory_.GetWeakPtr(), base::Owned(is_64bit)));

    bool *wallet_exe_exists = new bool();
    bool *wallet_temp_exe_exists = new bool();
    bool *wallet_instaler_exe_exists = new bool();

    base::ThreadPool::PostTaskAndReply(
        FROM_HERE,
        blocking_task_traits,
        base::BindOnce(&CheckWalletPaths, base::Unretained(wallet_exe_exists), base::Unretained(wallet_temp_exe_exists), base::Unretained(wallet_instaler_exe_exists)),
        base::BindOnce(&MessageHandler::OnCheckWalletPaths, weak_ptr_factory_.GetWeakPtr(), base::Owned(wallet_exe_exists), base::Owned(wallet_temp_exe_exists), base::Owned(wallet_instaler_exe_exists)));
}

void MessageHandler::HandleOpenFile(const base::ListValue *args)
{
    std::string name;
    args->GetString(0, &name);

    base::FilePath path;
    if ("Browser log" == name)
    {
        path = GetBrowserLogPath();
    }
    else if ("Wallet log" == name)
    {
        path = GetWalletLogPath();
    }

    platform_util::ShowItemInFolder(Profile::FromWebUI(web_ui()), path);
}

void MessageHandler::OnGotFilePaths(std::u16string* executable_path_data, std::u16string* profile_path_data)
{
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    base::DictionaryValue data;

    data.SetKey("Executable path", base::Value(*executable_path_data));
    data.SetKey("Profile path", base::Value(*profile_path_data));

	base::Value event_name("netboxinfo.filepaths");
	web_ui()->CallJavascriptFunctionUnsafe("wallet_send_event", std::move(event_name), std::move(data));
}


void MessageHandler::OnGotDisksFreeSpace(std::vector<std::pair<std::wstring, int64_t>> *data)
{
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    base::ListValue res;

    for (const auto &v : *data)
    {
        base::DictionaryValue entry;

        entry.SetKey("Path", base::Value(base::WideToUTF16(v.first)));
        entry.SetKey("Volume", base::Value(ui::FormatBytes(v.second)));

        res.Append(std::move(entry));
    }

	base::Value event_name("netboxinfo.diskfreespace");
    web_ui()->CallJavascriptFunctionUnsafe("wallet_send_event", std::move(event_name), std::move(res));
}

void MessageHandler::OnGotOperatingSystemType(bool *is_64)
{
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    std::string osBitCntStr = *is_64 ? "64 bit" : "32 bit";

    base::DictionaryValue data;
    data.SetKey("Operating System type", base::Value(osBitCntStr));

	base::Value event_name("netboxinfo.ostype");
    web_ui()->CallJavascriptFunctionUnsafe("wallet_send_event", std::move(event_name), std::move(data));
}

void MessageHandler::OnCheckWalletPaths(bool *wallet_exe_exists, bool *wallet_temp_exe_exists, bool *wallet_instaler_exe_exists)
{
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    base::DictionaryValue data;
    data.SetKey("Wallet exe exists", base::Value(*wallet_exe_exists));
    data.SetKey("Wallet temporary exe exists", base::Value(*wallet_temp_exe_exists));
    data.SetKey("Wallet installer exe exists", base::Value(*wallet_instaler_exe_exists));

	base::Value event_name("netboxinfo.walletpaths");
	web_ui()->CallJavascriptFunctionUnsafe("wallet_send_event", std::move(event_name), std::move(data));
}

base::RefCountedMemory* NetboxinfoUI::GetFaviconResourceBytes(ui::ScaleFactor scale_factor)
{
    return ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytesForScale(IDR_TAB_WALLET_LOGO_16, scale_factor);
}

NetboxinfoUI::NetboxinfoUI(content::WebUI* web_ui) : WebUIController(web_ui)
{
    Profile* profile = Profile::FromWebUI(web_ui);

    web_ui->AddMessageHandler(std::make_unique<MessageHandler>());

    content::WebUIDataSource* source = content::WebUIDataSource::Create(chrome::kChromeUINetboxinfoHost);

    source->AddResourcePath("netboxinfo.js",            IDR_NETBOXINFO_JS);
    source->AddResourcePath("netboxinfo_app.js",            IDR_NETBOXINFO_APP_JS);
    source->SetDefaultResource(IDR_NETBOXINFO_HTML_MAIN);

    source->DisableTrustedTypesCSP();

    content::WebUIDataSource::Add(profile, source);
    content::URLDataSource::Add(profile, std::make_unique<ThemeSource>(profile));
}
