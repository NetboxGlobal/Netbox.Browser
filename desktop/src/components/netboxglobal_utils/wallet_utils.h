#ifndef COMPONENTS_NETBOXGLOBAL_UTILS_WALLET_UTILS_H_
#define COMPONENTS_NETBOXGLOBAL_UTILS_WALLET_UTILS_H_

#include <string>
#include "base/files/file_path.h"

namespace Netboxglobal
{
base::FilePath::StringType get_wallet_filename();
base::FilePath get_wallet_data_folder();
bool get_wallet_exe_path(base::FilePath* path, const base::FilePath::StringType &prefix);
base::FilePath get_token_path();
std::string read_token_raw();
}

#endif