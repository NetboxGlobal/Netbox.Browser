#ifndef CHROME_BROWSER_BROWSER_PROCESS_UPDATE_INFO_H_
#define CHROME_BROWSER_BROWSER_PROCESS_UPDATE_INFO_H_

#include <stdint.h>
#include <memory>
#include <string>

#include "base/macros.h"
#include "base/version.h"
#include "chrome/browser/browser_update/browser_update.h"

class BrowserUpdateInfo {
public:
    BrowserUpdateInfo();
    ~BrowserUpdateInfo();

    void start(BrowserUpdate::info_callback info_callback);

    static std::string get_update_url();
    BrowserUpdate::FileMetadata get_file_metadata_from_json(const std::string& json_raw, int32_t &error_code);

    DISALLOW_COPY_AND_ASSIGN(BrowserUpdateInfo);
private:
    std::unique_ptr<network::SimpleURLLoader> json_fetcher_;
    BrowserUpdate::info_callback info_callback_;

    void on_json_ready(std::unique_ptr<std::string> response);
	bool is_waiting_for_restart(const std::string &version_new);
};

#endif  // CHROME_BROWSER_BROWSER_PROCESS_UPDATE_INFO_H_
    