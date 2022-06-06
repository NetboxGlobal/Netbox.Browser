#include "chrome/browser/browser_update/browser_update_executor.h"

#include <string>
#include <algorithm>
#include <cctype>

#include "base/base64.h"
#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/files/file.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/netboxglobal_utils/wallet_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_switches.h"
#include "crypto/secure_hash.h"
#include "crypto/sha2.h"
#include "net/base/load_flags.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

#if defined(OS_WIN)
#include "chrome/install_static/install_modes.h"
#include "chrome/installer/util/util_constants.h"
#endif

#if defined(OS_MAC)
#include "chrome/browser/browser_update/browser_update_helper.h"
#endif

BrowserUpdateExecutor::BrowserUpdateExecutor()
{
}

BrowserUpdateExecutor::~BrowserUpdateExecutor()
{
}

void BrowserUpdateExecutor::start(const BrowserUpdate::FileMetadata &file_metadata, BrowserUpdate::stop_callback callback)
{
    stop_callback_ = std::move(callback);

    file_metadata_ = file_metadata;

    std::transform(file_metadata_.update_hash.begin(), file_metadata_.update_hash.end(), file_metadata_.update_hash.begin(), ::toupper);

    auto resource_request         = std::make_unique<network::ResourceRequest>();

    // TODO :: check load flags

    resource_request->url         = GURL(file_metadata.update_url);
    resource_request->load_flags  = net::LOAD_DO_NOT_SAVE_COOKIES | net::LOAD_DISABLE_CACHE;
    resource_request->method      = "GET";

    Profile* profile = ProfileManager::GetLastUsedProfile();
    if (!profile)
    {
        if (!stop_callback_.is_null())
        {
            VLOG(NETBOX_LOG_LEVEL) << "no profile";
            std::move(stop_callback_).Run(UPDATE_STATUS::BROWSER_ERROR);
        }
        return;
    }

    VLOG(NETBOX_LOG_LEVEL) << "update process, download, start," << file_metadata.update_url;

    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory =
            profile->GetDefaultStoragePartition()->GetURLLoaderFactoryForBrowserProcess();
            //content::BrowserContext::GetDefaultStoragePartition(profile)->GetURLLoaderFactoryForBrowserProcess();

    file_fetcher_ = network::SimpleURLLoader::Create(std::move(resource_request), TRAFFIC_ANNOTATION_FOR_TESTS);
    file_fetcher_->DownloadToTempFile(url_loader_factory.get(), base::BindOnce(&BrowserUpdateExecutor::on_file_ready, base::Unretained(this)));
}

bool BrowserUpdateExecutor::is_file_valid()
{
    if (file_metadata_.update_hash.empty())
    {
        VLOG(NETBOX_LOG_LEVEL) << "update process, check file, hash is empty";
        return false;
    }

    base::File file(tmp_file_path_, base::File::FLAG_OPEN | base::File::FLAG_READ);

    if (!file.IsValid())
    {
        VLOG(NETBOX_LOG_LEVEL) << "update process, check file, file is not open " << tmp_file_path_;
        return false;
    }

    VLOG(NETBOX_LOG_LEVEL) << "update process, check file, " << tmp_file_path_ << ", " << file.GetLength();

    // calc hash for the file
    std::unique_ptr<crypto::SecureHash> ctx(crypto::SecureHash::Create(crypto::SecureHash::SHA256));

    char data_read[16384];

    int bytes_read = file.ReadAtCurrentPos(data_read, sizeof(data_read));
    while(bytes_read > 0)
    {
        ctx->Update(data_read, bytes_read);
        bytes_read = file.ReadAtCurrentPos(data_read, sizeof(data_read));
    }
    file.Close();

    std::string output(crypto::kSHA256Length, 0);
    ctx->Finish(base::data(output), output.size());

    if (base::HexEncode(output.data(), output.size()) != file_metadata_.update_hash)
    {
        VLOG(NETBOX_LOG_LEVEL) << "update process, check file, hash didn't match, file size " << file.GetLength();
        base::DeleteFile(tmp_file_path_);
        return false;
    }

    return true;
}

void BrowserUpdateExecutor::on_file_ready(base::FilePath path)
{
    tmp_file_path_ = path;

    VLOG(NETBOX_LOG_LEVEL) << "update process, download, ready";

    base::OnceClosure task = base::BindOnce(&BrowserUpdateExecutor::run_downloaded_file, base::Unretained(this));

    base::ThreadPool::PostTask(FROM_HERE,
        {
            base::MayBlock(),
            base::TaskPriority::BEST_EFFORT,
            base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN
        },
        std::move(task));
}

void BrowserUpdateExecutor::run_downloaded_file()
{

    VLOG(NETBOX_LOG_LEVEL) << "update process, run, check update file";

    // check file path and it's hash
    if (!is_file_valid())
    {
        VLOG(NETBOX_LOG_LEVEL) << "update process, check file error, filepath: " << tmp_file_path_.value() << " hash:" << file_metadata_.update_hash;

        if (!stop_callback_.is_null())
        {
            std::move(stop_callback_).Run(FILE_DOWNLOAD_ERROR);
        }
        else
        {
            VLOG(NETBOX_LOG_LEVEL) << "update process, check file error, stop callback is null";
        }

        return;
    }

    VLOG(NETBOX_LOG_LEVEL) << "update process, run, prestart";
    UPDATE_STATUS update_status = UNKNOWN_ERROR;

    // run and wait browser
    //
    #if defined(OS_WIN)
        base::CommandLine cmdline(tmp_file_path_);

        const base::CommandLine* browser_cmdline = base::CommandLine::ForCurrentProcess();
        if (browser_cmdline && browser_cmdline->HasSwitch(switches::kEnableLogging))
        {
            cmdline.AppendSwitch(installer::switches::kVerboseLogging);
            cmdline.AppendSwitchASCII(switches::kV, "1");
        }

        VLOG(NETBOX_LOG_LEVEL) << "update process, run, start, " << cmdline.GetCommandLineString();

        base::Process process = base::LaunchProcess(cmdline, base::LaunchOptions());
        if (!process.IsValid())
        {
            VLOG(NETBOX_LOG_LEVEL) << "update process, run, start error: " << cmdline.GetCommandLineString();

            update_status = FILE_START_ERROR;

        }
        else
        {
            VLOG(NETBOX_LOG_LEVEL) << "update process, run, waiting";

            int exit_code = 0;
            if (process.WaitForExit(&exit_code))
            {
                VLOG(NETBOX_LOG_LEVEL) << "update process, run, finished, exit code:" << exit_code;
            }
            else
            {
                VLOG(NETBOX_LOG_LEVEL) << "update process, run, finished, failed";
            }

            update_status = RESTART_REQUIRED;
        }

         base::DeleteFile(tmp_file_path_);
    #elif defined(OS_MAC)
        BrowserUpdateMacHelper::GetInstance()->set_dmg_path(tmp_file_path_);
        update_status = RESTART_REQUIRED;
    #else
        base::CommandLine cmdline(base::CommandLine::NO_PROGRAM);
    #endif

    if (!stop_callback_.is_null())
    {
        std::move(stop_callback_).Run(update_status);
    }
}