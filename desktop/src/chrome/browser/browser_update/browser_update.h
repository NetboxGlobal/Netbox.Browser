#ifndef CHROME_BROWSER_BROWSER_PROCESS_UPDATE_H_
#define CHROME_BROWSER_BROWSER_PROCESS_UPDATE_H_

#include <stdint.h>
#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/version.h"

#include "services/network/public/cpp/simple_url_loader.h"

enum UPDATE_STATUS{
    UNKNOWN_ERROR = 0,
    NO_UPDATE_REQUIRED,
    RESTART_REQUIRED,

    BROWSER_ERROR,    
    JSON_ERROR,
    FILE_DOWNLOAD_ERROR,
    FILE_START_ERROR
};

enum UPDATE_METADATA_STATE{
    UMS_OK = 100,
    UMS_NO_UPDATE_REQUIRED,
    UMS_UNKNOWN,
    UMS_ERR_NO_PROFILE,
    UMS_ERR_NO_FETCHER,
    UMS_ERR_EMPTY_JSON,
    UMS_ERR_INVALID_JSON,
    UMS_ERR_UNKNOWN_VERSION,
    UMS_ERR_INVALID_VERSION,
    UMS_ERR_UNKONWN_UPDATE_URL,
    UMS_ERR_UNKONWN_HASH,
    UMS_ERR_UNKONWN_CHECKSUM,
    UMS_ERR_INVALID_SIGNATURE,
    UMS_ERR_BASE64_DECODING_ERROR,
};

class BrowserUpdateInfo;
class BrowserUpdateExecutor;

class BrowserUpdate {
public:
    struct FileMetadata
    {
        std::string version;
        std::string update_url;
        std::string update_hash;
        std::string checksum;
        FileMetadata(const FileMetadata &val);
        FileMetadata();
        ~FileMetadata();
    };
    typedef base::OnceCallback<void(bool is_update_installed)>
        update_installed_callback;

    typedef base::OnceCallback<void(int32_t)>
        stop_callback;

    typedef base::OnceCallback<void(const FileMetadata &, int32_t)>
        info_callback;
        
        
    BrowserUpdate();
    ~BrowserUpdate();

    void start(update_installed_callback callback);
    void on_info_callback(const FileMetadata &metadata, int32_t result);

    void send_error_code(int32_t code);

    DISALLOW_COPY_AND_ASSIGN(BrowserUpdate);

private:   
    update_installed_callback callback_;
    void stop(int32_t status);

    std::unique_ptr<network::SimpleURLLoader> ping_event_fetcher_;
    void on_ping_completed(std::unique_ptr<std::string> s);  
    
    std::unique_ptr<BrowserUpdateInfo> update_info_;

    std::unique_ptr<BrowserUpdateExecutor> update_executor_;
};

#endif  // CHROME_BROWSER_BROWSER_PROCESS_UPDATE_H_