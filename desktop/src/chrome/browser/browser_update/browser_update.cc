//#include <windows.h>
//#include <Sddl.h>  

#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/storage_partition.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/chrome_version.h"
#include "chrome/browser/netbox/environment/controller/wallet_environment.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "net/base/load_flags.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/simple_url_loader.h"

#include "browser_update_info.h"
#include "browser_update_executor.h"
#include "browser_update.h"

BrowserUpdate::FileMetadata::FileMetadata() { }

BrowserUpdate::FileMetadata::~FileMetadata() { }

BrowserUpdate::FileMetadata::FileMetadata(const FileMetadata &val)
    : version(val.version), update_url(val.update_url), update_hash(val.update_hash), checksum(val.checksum)
{ }

BrowserUpdate::BrowserUpdate() { }

BrowserUpdate::~BrowserUpdate()
{
    ping_event_fetcher_.reset();
    update_info_.reset();
    update_executor_.reset();

    callback_.Reset();
}

void BrowserUpdate::start(update_installed_callback callback)
{
    VLOG(NETBOX_LOG_LEVEL) << "start " << CHROME_VERSION_STRING;

    callback_ = std::move(callback);
    
    update_info_.reset(new BrowserUpdateInfo());
    update_info_->start(base::BindOnce(&BrowserUpdate::on_info_callback, base::Unretained(this)));    
}

void BrowserUpdate::on_info_callback(const FileMetadata &meta, int32_t result)
{
    update_executor_.reset();
    
    if (UMS_OK != result && UMS_NO_UPDATE_REQUIRED != result)
    {
        VLOG(NETBOX_LOG_LEVEL) << "metadata error " << result;
        return stop(result);
    }

    if (UMS_NO_UPDATE_REQUIRED == result)
    {
        if (false == callback_.is_null())
        {
            std::move(callback_).Run(false);
        }
        return;
    }

    update_executor_.reset(new BrowserUpdateExecutor());
    update_executor_->start(meta, base::BindOnce(&BrowserUpdate::stop, base::Unretained(this)));
}

void BrowserUpdate::stop(int32_t status)
{
    if (!callback_.is_null())
    {
        if (UPDATE_STATUS::RESTART_REQUIRED == status)
        {
            VLOG(NETBOX_LOG_LEVEL) << "planning restart";
            std::move(callback_).Run(true);    
            return;
        }        
    }

    VLOG(NETBOX_LOG_LEVEL) << "add update error sending task to UI thread";

    base::PostTask(
        FROM_HERE,
        {content::BrowserThread::UI, base::MayBlock(), base::TaskPriority::BEST_EFFORT, base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
        base::BindOnce(&BrowserUpdate::send_error_code, base::Unretained(this), status)
    );
}

void BrowserUpdate::send_error_code(int32_t status)
{

    VLOG(NETBOX_LOG_LEVEL) << "make ping with error " << status;

    auto resource_request         = std::make_unique<network::ResourceRequest>();

    // TODO :: check load flags
    resource_request->url         = GURL(g_browser_process->env_controller()->get_api_url() + "/putlog?browser=" + CHROME_VERSION_STRING + "&error=" + std::to_string((int)status));
    resource_request->load_flags  = net::LOAD_DO_NOT_SAVE_COOKIES | net::LOAD_DISABLE_CACHE;    
    resource_request->method      = "GET";

    resource_request->headers.SetHeader("guid",    "");
    resource_request->headers.SetHeader("e",       std::to_string((int)status));
    resource_request->headers.SetHeader("version", CHROME_VERSION_STRING);
    
    //Profile* profile = ProfileManager::GetLastUsedProfile();    
    
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory = 
            ProfileManager::GetLastUsedProfile()->GetDefaultStoragePartition()->GetURLLoaderFactoryForBrowserProcess();
            //content::BrowserContext::GetDefaultStoragePartition(profile)->GetURLLoaderFactoryForBrowserProcess();

    ping_event_fetcher_ = network::SimpleURLLoader::Create(std::move(resource_request), TRAFFIC_ANNOTATION_FOR_TESTS);            

    ping_event_fetcher_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
        url_loader_factory.get(), base::BindOnce(&BrowserUpdate::on_ping_completed, base::Unretained(this)));
}

void BrowserUpdate::on_ping_completed(std::unique_ptr<std::string>)
{
    if (!callback_.is_null())
    {
        std::move(callback_).Run(false);
    }
}

