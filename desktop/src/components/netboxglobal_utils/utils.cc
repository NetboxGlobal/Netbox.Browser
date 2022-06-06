#include "components/netboxglobal_utils/utils.h"

#include "base/environment.h"
#include "base/files/file_util.h"
#include "base/logging.h"

#include <regex>
#include <vector>

namespace Netboxglobal
{

std::string filter_confidentional_data(const std::string &Json)
{
    std::regex rgx1("\"(hdseed|hdseedid|password|xpub)\":\\s*\"([a-z0-9]+)\"", std::regex_constants::icase);
    std::string str1 = std::regex_replace(Json, rgx1, "\"$1\":\"***\"");

    std::regex rgx2("\"(sethdseed|encryptwallet|walletpassphrase)\",\"params\":\\[.*\\]", std::regex_constants::icase);
    std::string str2 = std::regex_replace(str1, rgx2, "\"$1\",\"params\":***");

    std::regex rgx3("\\{\"result\":\".*\",\"error\":null,\"id\":null\\}", std::regex_constants::icase);
    return std::regex_replace(str2, rgx3, "{\"result\":\"***\",\"error\":null,\"id\":null}");
}

bool is_qa()
{
    std::unique_ptr<base::Environment> env(base::Environment::Create());

    return env->HasVar("NETBOXQA");
}

void preshowwallet()
{
    #if defined(OS_WIN)
        VLOG(NETBOX_LOG_LEVEL) << "preshowwallet";
        AllowSetForegroundWindow(ASFW_ANY);
    #endif
}


bool is_system_64bit()
{
    #if defined(OS_WIN)
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
    #else
        return true;
    #endif 
}


#if defined(OS_WIN)

bool send_hwnd_stop_signal_to_wallet(base::ProcessId pid)
{
    WindowsSearchParams params = { pid, L"Qt5QWindowIcon", false, true};

    find_window(params);

    if (params.result.empty())
    {
        VLOG(NETBOX_LOG_LEVEL) << L"wallet_launch, window not found";
        return false;
    }

    for (HWND wnd : params.result)
    {
		VLOG(NETBOX_LOG_LEVEL) << L"wallet_launch, WM_QUERYENDSESSION to " << wnd;
        ::PostMessage(wnd, WM_QUERYENDSESSION, 0, ENDSESSION_CLOSEAPP);
    }

    return true;
}

WindowsSearchParams::~WindowsSearchParams()
{}

WindowsSearchParams::WindowsSearchParams()
	: process_id(0), is_visible(true), is_main(true)
{}

WindowsSearchParams::WindowsSearchParams(base::ProcessId prc_id, const std::wstring &cls_name, bool visible, bool main)
	: process_id(prc_id), class_name(cls_name), is_visible(visible), is_main(main)
{}

WindowsSearchParams::WindowsSearchParams(base::ProcessId prc_id, const std::wstring &cls_name, const std::wstring &wnd_caption, bool visible, bool main)
    : process_id(prc_id), class_name(cls_name), window_caption(wnd_caption), is_visible(visible), is_main(main)
{}

static BOOL CALLBACK enum_windows_callback(HWND handle, LPARAM lParam)
{
	WindowsSearchParams *params = reinterpret_cast<WindowsSearchParams*>(lParam);

	DWORD process_id = 0;
	GetWindowThreadProcessId(handle, &process_id);

	if (params->process_id != process_id)
		return TRUE;

	wchar_t buffer[256];
	GetClassName(handle, buffer, 256);

	bool found = true;

	if (lstrcmpW(buffer, params->class_name.c_str()) != 0)
		found = false;

	if (params->is_main && GetWindow(handle, GW_OWNER) != 0)
		found = false;

	if (params->is_visible && !IsWindowVisible(handle))
		found = false;

    if (false == params->window_caption.empty())
    {
        GetWindowText(handle, buffer, 256);

        if (lstrcmpW(buffer, params->window_caption.c_str()) != 0)
        {
            found = false;
        }
    }

	if (found)
		params->result.push_back(handle);

	return TRUE;
}

void find_window(WindowsSearchParams &params)
{
	EnumWindows(enum_windows_callback, reinterpret_cast<LPARAM>(&params));
}

#endif

}