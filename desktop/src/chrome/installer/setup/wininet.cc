#include "chrome/installer/setup/wininet.h"

#include <vector>
#include <fstream>
#include <wininet.h>

static constexpr DWORD TIMEOUT_MSEC = 30 * 1000;
static constexpr DWORD CHUNK_LEN = 8192;

WinInet::~WinInet()
{
    if (0 != internet_)
        InternetCloseHandle(internet_);

    if (0 != connect_)
        InternetCloseHandle(connect_);

    if (0 != request_)
        InternetCloseHandle(request_);
}

std::wstring WinInet::user_agent()
{
    return L"Mozilla / 5.0";

}

DWORD WinInet::connect(const std::wstring &host, INTERNET_PORT port)
{
    port_ = port;

    internet_ = ::InternetOpen(user_agent().c_str(),
                               INTERNET_OPEN_TYPE_PRECONFIG,
                               nullptr,
                               nullptr,
                               0);
    if (nullptr == internet_)
    {
        return ::GetLastError();
    }

    DWORD timeout = TIMEOUT_MSEC;
    std::vector<DWORD> timeout_options = {
        INTERNET_OPTION_CONNECT_TIMEOUT,
        INTERNET_OPTION_SEND_TIMEOUT,
        INTERNET_OPTION_RECEIVE_TIMEOUT,
        INTERNET_OPTION_DATA_SEND_TIMEOUT,
        INTERNET_OPTION_DATA_RECEIVE_TIMEOUT
    };
    for (auto t : timeout_options)
    {
        if (FALSE == InternetSetOption(internet_, t, &timeout, sizeof(timeout)))
        { 
            return ::GetLastError(); 
        }
    }

    connect_ = ::InternetConnect(internet_,
                                 host.c_str(),
                                 port,
                                 nullptr,
                                 nullptr,
                                 INTERNET_SERVICE_HTTP,
                                 0,
                                 0);
    if (nullptr == connect_)
    {
        return ::GetLastError();
    }

    return S_OK;
}

DWORD WinInet::send_request(const std::wstring &verb,
                            const std::wstring &object,
                            const std::vector<char> &optional_data,
                            const std::map<std::wstring, std::wstring> &additional_headers)
{
    request_ = ::HttpOpenRequest(connect_,
                                 verb.c_str(),
                                 object.c_str(),
                                 nullptr,//version
                                 nullptr,//referer
                                 nullptr,//accept types
                                 (INTERNET_DEFAULT_HTTPS_PORT == port_) ? INTERNET_FLAG_SECURE : 0,
                                 0);
    if (nullptr == request_)
    {
        return ::GetLastError();
    }


    for (const auto& header : additional_headers)
    {
        const auto raw_header = header.first + L": " + header.second + L"\r\n";

        ::HttpAddRequestHeaders(request_,
                                raw_header.c_str(),
                                raw_header.size(),
                                HTTP_ADDREQ_FLAG_ADD | HTTP_ADDREQ_FLAG_REPLACE);
    }

    DWORD flags;
    DWORD opt_len = sizeof(flags);

    if (FALSE == ::InternetQueryOption(request_,
                                       INTERNET_OPTION_SECURITY_FLAGS,
                                       reinterpret_cast<LPVOID>(&flags),
                                       &opt_len))
    {
        return ::GetLastError();
    }

    flags |= SECURITY_FLAG_IGNORE_UNKNOWN_CA;
    flags |= SECURITY_FLAG_IGNORE_WRONG_USAGE;
    flags |= SECURITY_FLAG_IGNORE_CERT_CN_INVALID;
    flags |= SECURITY_FLAG_IGNORE_CERT_DATE_INVALID;

    if (FALSE == ::InternetSetOption(request_,
                                     INTERNET_OPTION_SECURITY_FLAGS,
                                     &flags,
                                     sizeof(flags)))
    {
        return ::GetLastError();
    }

    if (FALSE == ::HttpSendRequest(request_,
                                   nullptr,
                                   0,
                                   (false == optional_data.empty()) ? const_cast<char*>(optional_data.data()) : nullptr,
                                   (false == optional_data.empty()) ? optional_data.size() : 0))
    {
        return ::GetLastError();
    }

    DWORD buffer_len = sizeof(DWORD);
    if (FALSE == ::HttpQueryInfo(request_,
                                 HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER,
                                 &status_,
                                 &buffer_len,
                                 nullptr))
    {
        return ::GetLastError();
    }

    buffer_len = sizeof(DWORD);
    if (FALSE == ::HttpQueryInfo(request_,
                                 HTTP_QUERY_CONTENT_LENGTH | HTTP_QUERY_FLAG_NUMBER,
                                 &content_len_,
                                 &buffer_len,
                                 nullptr))
    {
        return ::GetLastError();
    }

    return S_OK;
}

DWORD WinInet::read_file(const std::function<DWORD(char *, size_t, WinInet *)> &on_read_chunk)
{
    char chunk[CHUNK_LEN];

    downloaded_ = 0;

    while (true)
    {
        DWORD bytes_readed = 0;

        if (FALSE == ::InternetReadFile(request_, chunk, CHUNK_LEN, &bytes_readed))
        {
            return ::GetLastError();
        }

        if (0 == bytes_readed)
        {
            return S_OK;
        }

        downloaded_ += bytes_readed;

        if (nullptr != on_read_chunk)
        {
            DWORD delegate_res = on_read_chunk(chunk, bytes_readed, this);
            if (S_OK != delegate_res)
            {
                return delegate_res;
            }
        }
    }

    return S_OK;
}