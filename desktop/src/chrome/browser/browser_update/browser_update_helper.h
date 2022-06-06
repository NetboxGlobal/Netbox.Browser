#ifndef CHROME_BROWSER_BROWSER_PROCESS_UPDATE_HELPER_H_
#define CHROME_BROWSER_BROWSER_PROCESS_UPDATE_HELPER_H_

#include "base/files/file_path.h"
#include "base/macros.h"

class BrowserUpdateMacHelper {
public:
    BrowserUpdateMacHelper();
    ~BrowserUpdateMacHelper();

    void set_dmg_path(base::FilePath path);
    bool is_set_dmg_path();
    void clear();
    void launch_updater();


    static BrowserUpdateMacHelper* GetInstance();
private:
    base::FilePath dmg_path_;

    DISALLOW_COPY_AND_ASSIGN(BrowserUpdateMacHelper);
};

#endif
