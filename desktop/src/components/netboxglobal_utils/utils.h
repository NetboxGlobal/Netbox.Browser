#ifndef COMPONENTS_NETBOXGLOBAL_UTILS_UTILS_H_
#define COMPONENTS_NETBOXGLOBAL_UTILS_UTILS_H_

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/process/process.h"

#include <string>

#if defined(OS_WIN)
#include <windows.h>
#endif 


namespace Netboxglobal
{

base::FilePath::StringType get_wallet_filename();
void preshowwallet();

std::string filter_confidentional_data(const std::string &Json);
bool is_qa();
bool is_system_64bit();

#if defined(OS_WIN)
using WindowId = HWND;

struct WindowsSearchParams
{
    base::ProcessId process_id;
	std::wstring class_name;
    std::wstring window_caption;
	std::vector<WindowId> result;
	bool is_visible;
	bool is_main;
	~WindowsSearchParams();
	WindowsSearchParams();
	WindowsSearchParams(base::ProcessId prc_id, const std::wstring &cls_name, bool visible, bool main);
    WindowsSearchParams(base::ProcessId prc_id, const std::wstring &cls_name, const std::wstring &wnd_caption, bool visible, bool main);
};

void find_window(WindowsSearchParams &Params);

bool send_hwnd_stop_signal_to_wallet(base::ProcessId pid);
#endif

}

#endif