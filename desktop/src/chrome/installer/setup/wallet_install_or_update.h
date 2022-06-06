#ifndef CHROME_INSTALLER_WALLET_INSTALLO_OR_UPDATE_H_
#define CHROME_INSTALLER_WALLET_INSTALLO_OR_UPDATE_H_

#include "base/path_service.h"
#include "base/version.h"

#include <string>

bool send_stop_signal_to_wallet_and_wait(const std::string& log_mode);
void wallet_install_or_update(const base::FilePath& target_path, const std::unique_ptr<base::Version> &installer_version);

#endif 
