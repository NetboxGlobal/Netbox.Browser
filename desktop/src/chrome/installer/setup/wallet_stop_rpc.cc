#include "chrome/installer/setup/wallet_stop_rpc.h"

#include "base/base64.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/installer/setup/wininet.h"
#include "components/netboxglobal_utils/utils.h"
#include "components/netboxglobal_utils/wallet_utils.h"
#include "url/gurl.h"

#include <string>

static const std::string RPC_URL = "http://127.0.0.1:28735";
static const std::string RPC_URL_QA = "http://127.0.0.1:28755";

static std::string get_post_data()
{
    base::DictionaryValue dict;
    dict.SetKey("jsonrpc", base::Value("1.0"));
    dict.SetKey("method", base::Value("stop"));
    dict.SetKey("params", base::ListValue());

    std::string json;
    base::JSONWriter::Write(dict, &json);

    return json;
}

static std::wstring get_auth_token()
{
    std::string wallet_token = Netboxglobal::read_token_raw();

    std::string access_token;
    base::Base64Encode(wallet_token, &access_token);

    return L"Basic " + base::SysUTF8ToWide(access_token);
}

DWORD WalletStopRPC::send(DWORD &http_status)
{
    GURL url = (true == Netboxglobal::is_qa()) ? GURL(RPC_URL_QA) : GURL(RPC_URL);

    WinInet inet;
    DWORD res = inet.connect(base::SysUTF8ToWide(url.host()), url.EffectiveIntPort());
    if (S_OK != res)
    {
        VLOG(0) << "wallet RPC, connect error, " << res << ", " << url.PathForRequest();
        return res;
    }

    std::string post_data = get_post_data();

    std::vector<char> post_data_raw = {post_data.begin(), post_data.end()};

    res = inet.send_request(L"POST",
                            base::SysUTF8ToWide(url.PathForRequest()),
                            post_data_raw,
                            {
                                {L"Authorization", get_auth_token()},
                                {L"Content-Type", L"application/json"}
                            });
    if (S_OK != res)
    {
        VLOG(0) << "wallet RPC, send error, " << res << ", " << url.PathForRequest() << ", token" << get_auth_token();
        return res;
    }

    http_status = inet.get_status();

    std::string res_str;

    auto read_func = [&res_str](char *data, size_t len, WinInet *sender)
    {
        res_str.append(data, len);
        return S_OK;
    };

    res = inet.read_file(read_func);

    VLOG(0) << "wallet RPC stop res: " << res_str;

    return res;
}
