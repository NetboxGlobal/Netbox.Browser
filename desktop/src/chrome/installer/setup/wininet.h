#ifndef CHROME_INSTALLER_SETUP_WININET_H_
#define CHROME_INSTALLER_SETUP_WININET_H_

#include <windows.h>
#include <string>
#include <vector>
#include <functional>
#include <map>

class WinInet
{
public:
    WinInet(){}
    ~WinInet();
    WinInet(const WinInet &) = delete;
    WinInet &operator= (const WinInet &) = delete;

    DWORD connect(const std::wstring &host, WORD port);
    DWORD send_request(const std::wstring &verb,
                       const std::wstring &object,
                       const std::vector<char> &optional_data = {},
                       const std::map<std::wstring, std::wstring> &additional_headers = {});
    DWORD read_file(const std::function<DWORD(char *, size_t, WinInet *)> &on_read_chunk);
    DWORD get_content_len() { return content_len_; }
    DWORD get_status() { return status_; }
    DWORD get_downloaded() { return downloaded_; }

private:
    std::wstring user_agent();
    DWORD content_len_ = 0;
    DWORD status_ = 0;
    DWORD downloaded_ = 0;
    WORD port_ = 0;
    LPVOID internet_ = 0;
    LPVOID connect_ = 0;
    LPVOID request_ = 0;
};

#endif