#include "chrome/browser/browser_update/browser_update_info.h"

#include "base/base64.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_update/browser_update.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_version.h"
#include "components/netboxglobal_verify/netboxglobal_verify.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/load_flags.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/resource_request.h"

#if defined(OS_WIN)
#include "chrome/install_static/install_modes.h"
#endif

#if defined(OS_MAC)
#include "chrome/browser/browser_update/browser_update_helper.h"
#endif


#if defined(OS_WIN)
static bool IsRunning32bitEmulatedOn64bit()
{    
    BOOL res = FALSE;
    #if defined(ARCH_CPU_X86)
        using IsWow64Process2Function = decltype(&IsWow64Process);

        IsWow64Process2Function is_wow64_process = reinterpret_cast<IsWow64Process2Function>(::GetProcAddress(::GetModuleHandleA("kernel32.dll"), "IsWow64Process"));
        if (false == is_wow64_process)
        {
            return false;
        }

        if (FALSE == is_wow64_process(::GetCurrentProcess(), &res))
        {
            return false;
        }
    #endif  // defined(ARCH_CPU_X86)
    return !!res;
}
#endif

BrowserUpdateInfo::BrowserUpdateInfo() 
{
}

BrowserUpdateInfo::~BrowserUpdateInfo() 
{
}

std::string BrowserUpdateInfo::get_update_url()
{
    #if defined(OS_WIN)
        return IsRunning32bitEmulatedOn64bit() ? std::string("https://cdn.netbox.global/update64.json") : std::string("https://cdn.netbox.global/update.json");
    #elif defined(OS_MAC)
        return std::string("https://cdn.netbox.global/update.mac.json");
    #else
        return "";
    #endif
}

void BrowserUpdateInfo::start(BrowserUpdate::info_callback info_callback)
{                                                    
    info_callback_ = std::move(info_callback);
        
    std::string json_url = BrowserUpdateInfo::get_update_url();

    VLOG(NETBOX_LOG_LEVEL) << "start " << json_url;

    auto resource_request         = std::make_unique<network::ResourceRequest>();

    resource_request->url         = GURL(json_url);
    resource_request->load_flags  = net::LOAD_DO_NOT_SAVE_COOKIES | net::LOAD_DISABLE_CACHE;    
    resource_request->method      = "GET";
    
    Profile* profile = ProfileManager::GetLastUsedProfile();    
    if (!profile)
    {
        if (!info_callback_.is_null())
        {            
			VLOG(NETBOX_LOG_LEVEL) << "no profile";
            std::move(info_callback_).Run({}, UMS_ERR_NO_PROFILE);
        }
        return;
    }
    
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory = 
            profile->GetDefaultStoragePartition()->GetURLLoaderFactoryForBrowserProcess() ;
            //content::BrowserContext::GetDefaultStoragePartition(profile)->GetURLLoaderFactoryForBrowserProcess();

    json_fetcher_ = network::SimpleURLLoader::Create(std::move(resource_request), TRAFFIC_ANNOTATION_FOR_TESTS);            
    if (json_fetcher_)
    {
        json_fetcher_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
            url_loader_factory.get(), base::BindOnce(&BrowserUpdateInfo::on_json_ready, base::Unretained(this)));

        return;
    }

    if (!info_callback_.is_null())
    {            
		VLOG(NETBOX_LOG_LEVEL) << "no json_fetcher_";
        std::move(info_callback_).Run({}, UMS_ERR_NO_FETCHER);
    }
}

// checking that update already installed
bool BrowserUpdateInfo::is_waiting_for_restart(const std::string &version_new)
{

#if defined(OS_WIN)
    base::FilePath path_new_dll;
    if (base::PathService::Get(base::DIR_LOCAL_APP_DATA, &path_new_dll))
    {
        path_new_dll = path_new_dll.Append(install_static::kProductPathName).Append(FILE_PATH_LITERAL("Application")).Append(base::UTF8ToWide(version_new).c_str()).Append(FILE_PATH_LITERAL("chrome.dll"));
        if (base::PathExists(path_new_dll))
        {
            return true;
        }
    }
#endif    

#if defined(OS_MAC)
    if (BrowserUpdateMacHelper::GetInstance()->is_set_dmg_path())
    {
        return true;
    }
#endif

    return false;
}


#define RETURN_IF_TRUE(statement, message, err_code) \
{\
    if (true == (statement))\
    {\
        VLOG(NETBOX_LOG_LEVEL) << message;\
        error_code = (err_code);\
        return {};\
    }\
}\

BrowserUpdate::FileMetadata BrowserUpdateInfo::get_file_metadata_from_json(const std::string& json_raw, int32_t &error_code)
{
    RETURN_IF_TRUE(json_raw.empty(), "json empty", UMS_ERR_EMPTY_JSON)

    // parsing json
    std::unique_ptr<base::Value> json = base::JSONReader::ReadDeprecated(json_raw);
    base::DictionaryValue* root = nullptr;
    
    RETURN_IF_TRUE(!json || !json->is_dict() || !json->GetAsDictionary(&root) || !root, "json not valid", UMS_ERR_INVALID_JSON)

    BrowserUpdate::FileMetadata meta;

    RETURN_IF_TRUE(!root->GetString("version", &meta.version) || meta.version.empty(), "version empty", UMS_ERR_UNKNOWN_VERSION)
    RETURN_IF_TRUE(!root->GetString("url", &meta.update_url) || meta.update_url.empty(), "update_url empty", UMS_ERR_UNKONWN_UPDATE_URL)
    RETURN_IF_TRUE(!root->GetString("hash", &meta.update_hash) || meta.update_hash.empty(), "hash empty", UMS_ERR_UNKONWN_HASH)
    RETURN_IF_TRUE(!root->GetString("checksum", &meta.checksum) || meta.checksum.empty(), "checksum empty", UMS_ERR_UNKONWN_HASH)

    // get new version
    base::Version version_new(meta.version);
    RETURN_IF_TRUE(!version_new.IsValid(), "invalid version " << meta.version, UMS_ERR_INVALID_VERSION)

    std::string base64_decoded_sig;
    RETURN_IF_TRUE(!base::Base64Decode(meta.checksum, &base64_decoded_sig), "checksum base64 decoding error", UMS_ERR_BASE64_DECODING_ERROR);

    // verify checksum
    std::string data = meta.version + meta.update_url + meta.update_hash;
    bool sig_verify_res = verify_signature(base::as_bytes(base::make_span(base64_decoded_sig)), base::as_bytes(base::make_span(data)));
    RETURN_IF_TRUE(!sig_verify_res, "invalid signature " << meta.checksum, UMS_ERR_INVALID_SIGNATURE)

    // verify version
    base::Version version_browser_(CHROME_VERSION_STRING);
    RETURN_IF_TRUE(version_new.CompareTo(version_browser_) <= 0, "no update required", UMS_NO_UPDATE_REQUIRED)
    
    RETURN_IF_TRUE(is_waiting_for_restart(meta.version), "waiting for restart", UMS_NO_UPDATE_REQUIRED)

    error_code = UMS_OK;

    VLOG(NETBOX_LOG_LEVEL) << "updating browser to " << meta.version << " using " << meta.update_url << " and hash " << meta.update_hash;

    return meta;
}

void BrowserUpdateInfo::on_json_ready(std::unique_ptr<std::string> response)    
{
    VLOG(NETBOX_LOG_LEVEL) << "get response";

    if (info_callback_.is_null() || nullptr == response)
    {
        return;
    }

    VLOG(NETBOX_LOG_LEVEL) << "get json response";

    int32_t res = UMS_UNKNOWN;
    BrowserUpdate::FileMetadata file_metadata = get_file_metadata_from_json(response->c_str(), res);
    
    std::move(info_callback_).Run(file_metadata, res);
}

    
