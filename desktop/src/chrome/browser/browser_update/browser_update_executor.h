#ifndef CHROME_BROWSER_BROWSER_PROCESS_UPDATE_EXECUTOR_H_
#define CHROME_BROWSER_BROWSER_PROCESS_UPDATE_EXECUTOR_H_

#include <stdint.h>
#include <memory>
#include <string>

#include "base/macros.h"
#include "base/files/file_util.h"

#include "chrome/browser/browser_update/browser_update.h"

class BrowserUpdateExecutor {
public:
    BrowserUpdateExecutor();
    ~BrowserUpdateExecutor();

    void start(const BrowserUpdate::FileMetadata &metadata, BrowserUpdate::stop_callback callback);

    DISALLOW_COPY_AND_ASSIGN(BrowserUpdateExecutor);
private:
	BrowserUpdate::FileMetadata file_metadata_;
	base::FilePath tmp_file_path_;
    std::unique_ptr<network::SimpleURLLoader> file_fetcher_;

    BrowserUpdate::stop_callback stop_callback_;

	void on_file_ready(base::FilePath path);
	void run_downloaded_file();
    bool is_file_valid();
};

#endif  // CHROME_BROWSER_BROWSER_PROCESS_UPDATE_EXECUTOR_H_
    