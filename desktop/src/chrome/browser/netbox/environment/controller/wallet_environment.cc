#include <regex>

#include "chrome/browser/netbox/environment/controller/wallet_environment.h"

#include "base/hash/md5.h"
#include "base/base64.h"
#include "base/base64url.h"
#include "base/guid.h"
#include "base/threading/platform_thread.h"
#include "base/containers/span.h"
#include "base/json/json_reader.h"
#include "base/strings/utf_string_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/system/sys_info.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/system/sys_info.h"
#include "chrome/browser/browser_process_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/netbox/activity/activity_watcher.h"
#include "components/netboxglobal_verify/decode_public_key.h"
#include "components/netboxglobal_utils/utils.h"
#include "components/netboxglobal_utils/wallet_utils.h"
#include "chrome/browser/netbox/environment/launch/wallet_launch_helper.h"
#include "components/netboxglobal_verify/netboxglobal_verify.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/browser_task_traits.h"
#include "crypto/signature_verifier.h"
#include "crypto/rsa_encrypt.h"
#include "net/base/escape.h"
#include "net/base/mime_util.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_options.h"
#include "net/cookies/cookie_store.h"
#include "net/cookies/cookie_util.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_context.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

#include "base/json/json_writer.h" // remove netboxtodo

//NETBOXTODO UNIX port
#if defined(OS_WIN)
#include "chrome/install_static/install_modes.h"
#include "base/win/registry.h"
#include <windows.h>
#include <Objbase.h>

std::wstring get_promo_registry_key()
{
    return std::wstring(L"Software\\") + install_static::kProductPathName + L"\\promocode";
}

static const int32_t MAX_PROMOCODE_LEN = 64;
#endif

#define BOUNDARY_SEQUENCE "----**--yradnuoBgoLtrapitluMklaTelgooG--**----"

#define AUTH_URL "https://account.netbox.global"
#define DEV_AUTH_URL "https://devaccount.netbox.global"

#define COOKIE_URL "https://netbox.global"

#define RPC_URL "http://127.0.0.1:28735"
#define RPC_URL_QA "http://127.0.0.1:28755"

#define API_URL "https://api.netbox.global"
#define API_URL_QA "https://devapi.netbox.global"

namespace Netboxglobal
{

WalletSessionManager::Data::Data()
{
}

WalletSessionManager::Data::~Data()
{
}

WalletSessionManager::WalletSessionManager()
{
    is_qa_ = false;
    if (true == Netboxglobal::is_qa())
    {
        is_qa_ = true;
    }

    is_debug_ = false;
    const auto* command_line = base::CommandLine::ForCurrentProcess();

    if (command_line->HasSwitch("debug-netbox"))
    {
        is_debug_ = true;
    }

    std::atomic_init(&wallet_restarting_, false);
}

WalletSessionManager::~WalletSessionManager()
{
    stop();
}

std::string WalletSessionManager::get_api_url() const
{
    return is_qa() ? API_URL_QA : API_URL;
}

std::string WalletSessionManager::get_rpc_url() const
{
    return is_qa() ? RPC_URL_QA : RPC_URL;
}

std::string WalletSessionManager::get_auth_url() const
{
    return is_qa() ? DEV_AUTH_URL : AUTH_URL;
}

std::string WalletSessionManager::get_cookie_url() const
{
    return COOKIE_URL;
}

std::string WalletSessionManager::get_cookie_token_name() const
{
    return is_qa() ? "devtoken" : "token";
}


void WalletSessionManager::get_wallet_rpc_credentails(std::string &auth_token)
{
    std::lock_guard<std::mutex> grd(wallet_rpc_token_mutex_);
    auth_token = token_base64_;
}

const std::string &WalletSessionManager::get_guid() const
{
    return data_.guid;
}

const std::string &WalletSessionManager::get_encrypted_machine_id() const
{
    return encrypted_machine_id_;
}

const std::string &WalletSessionManager::get_wallet_exe_creation_time() const
{
    return wallet_exe_creation_time_;
}

const ExtendedHardwareInfo &WalletSessionManager::get_hardware_info() const
{
    return hardware_info_;
}

bool WalletSessionManager::is_qa() const
{
    return is_qa_;
}

bool WalletSessionManager::is_debug() const
{
    return is_debug_;
}

bool WalletSessionManager::is_first_launch() const
{
    return is_first_launch_;
}

WalletSessionManager::AuthState WalletSessionManager::get_auth_state() const
{
	return auth_state_;
}

std::vector<std::string> WalletSessionManager::encrypt_data(const std::string &data) const
{
    std::string public_key = decode_public_key();
    if (public_key.empty())
    {
        VLOG(NETBOX_LOG_LEVEL) << L"cant decode public key";
        return {};
    }

    std::vector<std::vector<uint8_t>> enc_data;
    if (false == crypto::RSAEncrypt(base::as_bytes(base::make_span(public_key)),
                                    base::as_bytes(base::make_span(data)),
                                    enc_data))
    {
        VLOG(NETBOX_LOG_LEVEL) << L"cant encrypt request";
        return {};
    }

    std::vector<std::string> res;
    for (const auto &b : enc_data)
    {
        std::string base64_str;
        base::Base64Encode(std::string({b.begin(), b.end()}), &base64_str);

        res.push_back(base64_str);
    }

    return res;
}

std::string WalletSessionManager::create_api_request(const std::string &data_json) const
{
    std::string multipart_data;
    for (const std::string &b : encrypt_data(data_json))
    {
        net::AddMultipartValueForUpload("data[]", b, BOUNDARY_SEQUENCE, "", &multipart_data);
    }

    return multipart_data;
}

void WalletSessionManager::change_auth_state(AuthState auth_state)
{
	VLOG(1) << "cookie state change:" << auth_state;
	
    auth_state_ = auth_state;
    notify_state_observers();
}

void WalletSessionManager::change_init_state(DataState data_state)
{
	VLOG(1) << "init state change" << data_state;
	
    init_state_ = data_state;
    notify_state_observers();
}

void WalletSessionManager::change_wallet_state(DataState data_state)
{
	VLOG(1) << "wallet state change" << data_state;	
	
    wallet_state_ = data_state;
    notify_state_observers();
}

void WalletSessionManager::notify_state_observers()
{
	if (AUTH_UNDEFINED == auth_state_)
	{
        for (const auto &observer : data_observers_list_)
        {
            observer(DS_NONE, data_);
        }		
	}
	
    if (AUTH_NOT_LOGGED == auth_state_)
    {
        for (const auto &observer : data_observers_list_)
        {
            observer(DS_AUTH_COOKIE_ERROR, data_);
        }
        return;
    }

    if (wallet_state_ != DS_NONE && wallet_state_ != DS_OK)
    {
        for (const auto &observer : data_observers_list_)
        {
            observer(wallet_state_, data_);
        }
        return;
    }

    if (init_state_ != DS_NONE && init_state_ != DS_OK)
    {
        for (const auto &observer : data_observers_list_)
        {
            observer(init_state_, data_);
        }
        return;
    }

    if (init_state_ == DS_OK && wallet_state_ == DS_OK)
    {
        for (const auto &observer : data_observers_list_)
        {
            observer(DS_OK, data_);
        }
        return;
    }
}

void WalletSessionManager::add_data_observer(EventObserversList::value_type val) const
{
    data_observers_list_.push_back(val);
}

network::mojom::CookieManager* WalletSessionManager::get_cookie_manager()
{

    Profile* profile = ProfileManager::GetLastUsedProfile();
    if (!profile)
    {
        VLOG(NETBOX_LOG_LEVEL) << L"can't get profile";
        return nullptr;
    }

    
    return profile->GetDefaultStoragePartition()->GetCookieManagerForBrowserProcess();
}

std::unique_ptr<net::CanonicalCookie> WalletSessionManager::get_cookie_helper(const std::string& url, const std::string& domain, const std::string& name, const std::string& value)
{

    base::Time current_time     = base::Time::Now();
    base::Time expiration_time  = current_time + base::TimeDelta::FromDays(10 * 365);

    return net::CanonicalCookie::CreateSanitizedCookie(
        GURL(url),                          // GURL& url
        name,                               // std::string name
        value,                              // std::string value
        domain,                             // std::string domain
        "/",                                // std::string path
        current_time,                       // creation_time,
        expiration_time,                    // expiration_time
        current_time,                       // last_access_time
        true,                               // bool secure?
        true,                               // bool http_only
        net::CookieSameSite::NO_RESTRICTION,// CookieSameSite same_site, may be LAX or STRICT mode?
        net::CookiePriority::COOKIE_PRIORITY_HIGH,
        false                               // same_party
    );
}

std::u16string WalletSessionManager::get_promo_code()
{
    
//NETBOXTODO UNIX port
#if defined(OS_WIN)
    std::wstring promo;
    base::win::RegKey promo_reg_key;

    if (ERROR_SUCCESS != promo_reg_key.Open(HKEY_CURRENT_USER, get_promo_registry_key().c_str(), KEY_READ))
    {
        return u"";
    }

    promo_reg_key.ReadValue(L"value", &promo);

    if (promo.length() > MAX_PROMOCODE_LEN)
    {
        VLOG(NETBOX_LOG_LEVEL) << "promocode length exceeded";
        return u"";
    }

    return base::WideToUTF16(promo);    
#else
    return u"";
#endif    
}

void WalletSessionManager::set_session_cookie()
{
    VLOG(NETBOX_LOG_LEVEL) << "session, start";

    std::string session_ = regex_replace(base::ToUpperASCII(base::GenerateGUID()), std::regex("-"), "");

    std::unique_ptr<net::CanonicalCookie> cookie = get_cookie_helper(
        get_cookie_url(),
        ".netbox.global",
        "session",
        session_
    );

    if (!cookie.get())
    {
        VLOG(NETBOX_LOG_LEVEL) << L"session, failed to create cookie";
        change_init_state(DS_SESSION_ERROR);
        return;
    }

    network::mojom::CookieManager* cookie_manager = get_cookie_manager();
    if (!cookie_manager)
    {
        VLOG(NETBOX_LOG_LEVEL) << L"session, failed to get cookie_manager";
        change_init_state(DS_SESSION_ERROR);
        return;
    }

    net::CookieOptions options;
    options.set_include_httponly();

    cookie_manager->SetCanonicalCookie(
        *cookie.get(),
        net::cookie_util::SimulatedCookieSource(*cookie, "https"),
        options,
        base::BindOnce(&WalletSessionManager::on_set_session_cookie, base::Unretained(this)));
}

void WalletSessionManager::on_set_session_cookie(net::CookieAccessResult set_cookie_result)
{
    if (!set_cookie_result.status.IsInclude())
    {
        VLOG(NETBOX_LOG_LEVEL) << L"session, failed to set cookie";
        change_init_state(DS_SESSION_ERROR);
		return;
    }    
    	
    VLOG(NETBOX_LOG_LEVEL) << L"start, 4/6, session, cookie was created";

    base::PostTask(
	    FROM_HERE,
	    {content::BrowserThread::UI},
	    base::BindOnce(&WalletSessionManager::process_guid, base::Unretained(this))
    );
}

void WalletSessionManager::process_guid()
{
    network::mojom::CookieManager* cookie_manager = get_cookie_manager();
    if (!cookie_manager)
    {
        VLOG(NETBOX_LOG_LEVEL) << L"failed to get cookie_manager for session";
        change_init_state(DS_GUID_ERROR);
        return;
    }

    net::CookieOptions options;
    options.set_include_httponly();
    options.set_return_excluded_cookies();

    cookie_manager->GetCookieList(
        GURL(get_cookie_url()),
        options,
        base::BindOnce(&WalletSessionManager::on_get_guid_from_cookie, base::Unretained(this))
    );
}

bool WalletSessionManager::read_guid_from_cookies(const net::CookieAccessResultList &cookies)
{
    std::string guid_raw, guid_checksum_raw;

    for (const net::CookieWithAccessResult& cookie_with_access_result : cookies)
    {
        if (cookie_with_access_result.cookie.Name() == "browserguid")
        {
            guid_raw = cookie_with_access_result.cookie.Value();
        }

        if (cookie_with_access_result.cookie.Name() == "browserguidsign")
        {
            guid_checksum_raw = cookie_with_access_result.cookie.Value();
        }
    }


    if (true == guid_raw.empty())
    {
        VLOG(NETBOX_LOG_LEVEL) << L"guid, not found, created request";
        return false;
    }

    if (true == guid_checksum_raw.empty())
    {
        VLOG(NETBOX_LOG_LEVEL) << L"guid, sign not found, created request";
        return false;
    }

    std::string guid_checksum_raw_decoded;
    base::Base64Decode(guid_checksum_raw, &guid_checksum_raw_decoded);

    if (!verify_signature(base::as_bytes(base::make_span(guid_checksum_raw_decoded)), base::as_bytes(base::make_span(guid_raw))))
    {
        VLOG(NETBOX_LOG_LEVEL) << L"guid, invalid checksum, created request";
        return false;
    }

	data_.guid = guid_raw;
    return true;
}

void WalletSessionManager::on_get_guid_from_cookie(const net::CookieAccessResultList& cookies, const net::CookieAccessResultList&)
{
    if (read_guid_from_cookies(cookies))
    {
        io_get_auth_cookie();

        return;
    }

    base::PostTask(
        FROM_HERE,
        {content::BrowserThread::UI},
        base::BindOnce(&WalletSessionManager::send_new_guid_request, base::Unretained(this))
    );

}

void WalletSessionManager::send_new_guid_request()
{
    is_first_launch_ = true;

    base::Value params(base::Value::Type::DICTIONARY);
    params.SetKey("machine_id",     base::Value(data_.machine_id));
    params.SetKey("version",        base::Value("1"));
    params.SetKey("referral_id",    base::Value(get_promo_code()));

    std::unique_ptr<Netboxglobal::WalletHttpCallSignature> signature(new Netboxglobal::WalletHttpCallSignature(Netboxglobal::WalletHttpCallType::API_SIGNED));
    signature->set_method_name("get_guid");
    signature->set_params(std::move(params));

    // todo is_qa signature->

    std::unique_ptr<WalletRequest> http_request = std::make_unique<WalletRequest>();
    WalletRequest* http_request_ptr = http_request.get();

    http_request->start(std::move(signature),
        base::BindOnce(&WalletSessionManager::on_guid_request_response, base::Unretained(this)));

    ui_requests_[http_request_ptr] = std::move(http_request);
}

void WalletSessionManager::on_guid_request_response(std::unique_ptr<WalletHttpCallSignature> signature, base::Value data, WalletRequest* http_request_ptr)
{
    auto it = ui_requests_.find(http_request_ptr);
    if (it == ui_requests_.end())
    {
        LOG(WARNING) << "failed to find http_request, " << http_request_ptr;
    }
    else
    {
        std::unique_ptr<WalletRequest> http_request = std::move(it->second);
        ui_requests_.erase(it);
    }

    if (data.FindKey("error"))
    {
        std::string json;
        base::JSONWriter::Write(data, &json);

        if (guid_request_interval_seconds_ < 600)
        {
            guid_request_interval_seconds_ = guid_request_interval_seconds_ * 2;
        }

        base::PostDelayedTask(
            FROM_HERE,
            {
                content::BrowserThread::UI,
                base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN
            },
            base::BindOnce(&WalletSessionManager::send_new_guid_request, base::Unretained(this)),
            base::TimeDelta::FromSeconds(guid_request_interval_seconds_));

        return;
    };

    std::string* guid_raw = data.FindStringKey("guid");
    std::string* checksum_raw_encoded = data.FindStringKey("checksum");

    if (!guid_raw || !checksum_raw_encoded)
    {
        change_init_state(DS_GUID_ERROR);
        return;
    }

    data_.guid = std::string(guid_raw->c_str());
    data_.guid_checksum = std::string(checksum_raw_encoded->c_str());

    std::string checksum_raw_decoded;
    base::Base64Decode(data_.guid_checksum, &checksum_raw_decoded);

    if (!verify_signature(base::as_bytes(base::make_span(checksum_raw_decoded)), base::as_bytes(base::make_span(data_.guid))))
    {
        data_.guid = "";
        data_.guid_checksum = "";

        change_init_state(DS_GUID_ERROR);
        return;
    }

    set_data_to_cookie("browserguid", data_.guid, base::BindOnce(&WalletSessionManager::on_set_guid_cookie_result, base::Unretained(this)));
}

void WalletSessionManager::set_data_to_cookie(const std::string &variable,
                                       const std::string &value,
                                       network::mojom::CookieManager::SetCanonicalCookieCallback &&callback)
{
    network::mojom::CookieManager* cookie_manager = get_cookie_manager();
    if (!cookie_manager)
    {
        VLOG(NETBOX_LOG_LEVEL) << L"failed to get cookie_manager for guid";
        change_init_state(DS_GUID_ERROR);
        return;
    }

    std::unique_ptr<net::CanonicalCookie> cookie = get_cookie_helper(
        get_cookie_url(),
        ".netbox.global",
        variable,
        value
    );

    if (!cookie.get())
    {
        VLOG(NETBOX_LOG_LEVEL) << L"failed to create GUID cookie";
        change_init_state(DS_GUID_ERROR);
        return;
    }

    net::CookieOptions options;
    options.set_include_httponly();
    cookie_manager->SetCanonicalCookie(
        *cookie.get(),
        net::cookie_util::SimulatedCookieSource(*cookie, "https"),
        options,
        std::move(callback));
}


void WalletSessionManager::on_set_guid_cookie_result(net::CookieAccessResult set_cookie_result)
{
    if (set_cookie_result.status.IsInclude())
    {
        VLOG(NETBOX_LOG_LEVEL) << L"GUID cookie was created";
        set_data_to_cookie("browserguidsign", data_.guid_checksum, base::BindOnce(&WalletSessionManager::on_set_guid_sign_cookie_result, base::Unretained(this)));   
    }
    else
    {
        VLOG(NETBOX_LOG_LEVEL) << L"failed to set GUID cookie";
        change_init_state(DS_GUID_ERROR);
    }
}

void WalletSessionManager::on_set_guid_sign_cookie_result(net::CookieAccessResult set_cookie_result)
{
    if (set_cookie_result.status.IsInclude())
    {
        VLOG(NETBOX_LOG_LEVEL) << L"GUID cookie checksum was created";

        io_get_auth_cookie();
    }
    else
    {
        VLOG(NETBOX_LOG_LEVEL) << L"failed to set GUID checksum cookie";
        change_init_state(DS_GUID_ERROR);
    }
}

void WalletSessionManager::io_get_auth_cookie()
{
    network::mojom::CookieManager* cookie_manager = get_cookie_manager();
    if (!cookie_manager)
    {
        VLOG(NETBOX_LOG_LEVEL) << L"failed to get cookie_manager for get auth";
        change_auth_state(AUTH_NOT_LOGGED);
        return;
    }

    VLOG(NETBOX_LOG_LEVEL) << "start, 5/6, checking auth cookie";

    net::CookieOptions options;
    options.set_include_httponly();
    options.set_return_excluded_cookies();
    options.set_same_site_cookie_context(net::CookieOptions::SameSiteCookieContext(net::CookieOptions::SameSiteCookieContext::ContextType::SAME_SITE_STRICT));

    cookie_manager->GetCookieList(
        GURL(get_cookie_url()),
        options,
        base::BindOnce(&WalletSessionManager::on_get_auth_cookies_list, base::Unretained(this))
    );
}

void WalletSessionManager::on_get_auth_cookies_list(const net::CookieAccessResultList& cookies, const net::CookieAccessResultList&)
{
    std::string cookie_token_name = get_cookie_token_name();

    bool has_auth_cookie = false;
    for (const net::CookieWithAccessResult& cookie_with_access_result : cookies)
    {
        if (cookie_with_access_result.cookie.Name() == cookie_token_name && false == cookie_with_access_result.cookie.IsExpired(base::Time::Now()))
        {
            has_auth_cookie = true;
            VLOG(NETBOX_LOG_LEVEL) << L"start, " << cookie_token_name << " cookie found";
        }
    }

	if (has_auth_cookie)
	{
		change_auth_state(AUTH_LOGGED);
	}		
	else
	{
		change_auth_state(AUTH_NOT_LOGGED);
	}

    if (!has_auth_cookie)
    {
        VLOG(NETBOX_LOG_LEVEL) << "start, " << cookie_token_name << " cookie not found";
    }

    create_auth_cookie_subscription();
}

void WalletSessionManager::create_auth_cookie_subscription()
{
    network::mojom::CookieManager* cookie_manager = get_cookie_manager();
    if (!cookie_manager)
    {
        VLOG(NETBOX_LOG_LEVEL) << "cookie change handler, configure failed";
        change_auth_state(AUTH_NOT_LOGGED);

        return;
    }

    cookie_manager->AddCookieChangeListener(
        GURL(get_cookie_url()),
        get_cookie_token_name(),
        cookie_listener_binding_.BindNewPipeAndPassRemote());

    VLOG(NETBOX_LOG_LEVEL) << "start, 6/6, configured on_change handler for " << get_cookie_token_name() ;
		
	change_init_state(DS_OK);
}

void WalletSessionManager::OnCookieChange(const net::CookieChangeInfo& change)
{
    VLOG(NETBOX_LOG_LEVEL) << "cookie changed " << change.cookie.Name() << " "
                                                << change.cookie.Domain() << " "
                                                << change.cookie.Path() << " "
                                                << change.cookie.IsExpired(base::Time::Now())
                                                << " cause:" << static_cast<int32_t>(change.cause);

    if (change.cookie.IsExpired(base::Time::Now())
       || (net::CookieChangeCause::INSERTED != change.cause && net::CookieChangeCause::OVERWRITE != change.cause)
    )
    {
        VLOG(NETBOX_LOG_LEVEL) << "on_cookie_changed, logout";
        is_first_launch_ = false;
		change_auth_state(AUTH_NOT_LOGGED);
    }
    else
    {
        VLOG(NETBOX_LOG_LEVEL) << "on_cookie_changed, logged in";
        change_auth_state(AUTH_LOGGED);
    }	
}

void WalletSessionManager::on_wallet_start(DataState state, std::string token, std::string wallet_exe_creation_time)
{   
    wallet_restarting_.store(false); 
    {
        wallet_exe_creation_time_ = wallet_exe_creation_time;

        std::lock_guard<std::mutex> grd(wallet_rpc_token_mutex_);
        base::Base64Encode(token, &token_base64_);
    }
    change_wallet_state(state);
}

void WalletSessionManager::wallet_start(WALLET_LAUNCH mode)
{
    VLOG(NETBOX_LOG_LEVEL) << "schedule wallet_launch: " << mode;

    bool expected_value = false;
    if (false == wallet_restarting_.compare_exchange_strong(expected_value, true))
    {
        return;
    }

    base::ThreadPool::PostTask(
        FROM_HERE,
        {
            base::MayBlock(),
            base::WithBaseSyncPrimitives(),
            base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN
        },
        base::BindOnce(&wallet_launch,  mode)
    );
}

void WalletSessionManager::start()
{
    VLOG(NETBOX_LOG_LEVEL) << L"start, 0/6";
    wallet_start(WALLET_LAUNCH::START);

    VLOG(NETBOX_LOG_LEVEL) << L"start, 1/6, getting hardware";

    // get hardware information
    Netboxglobal::get_hardware_info(base::BindOnce(&WalletSessionManager::on_get_hardware_info, base::Unretained(this)));
}

void WalletSessionManager::on_get_hardware_info(ExtendedHardwareInfo info)
{
    VLOG(NETBOX_LOG_LEVEL) << L"start, 2/6, starting activity watcher";

    hardware_info_ = std::move(info);

    // calc machine_id
    base::MD5Context md5_ctx;

    base::MD5Init(&md5_ctx);

    base::MD5Update(&md5_ctx, hardware_info_.computer_system_manufacturer);
    base::MD5Update(&md5_ctx, hardware_info_.computer_system_model);
    base::MD5Update(&md5_ctx, hardware_info_.bios_serial_number);

    base::MD5Digest md5_dgst;
    base::MD5Final(&md5_dgst, &md5_ctx);

    data_.machine_id = base::MD5DigestToBase16(md5_dgst);

    // creating encrypted machine_id

    std::string machine_descr = Netboxglobal::get_machine_description(hardware_info_);

    encrypted_machine_id_ = "";
    for (const std::string &val : encrypt_data(machine_descr))
        encrypted_machine_id_ += val;

    Netboxglobal::Monitoring::ActivityWatcher::get_instance()->start(this);


    VLOG(NETBOX_LOG_LEVEL) << "start, 3/6, adding set session cookie task";

    base::PostTask(
        FROM_HERE,
        {
            content::BrowserThread::UI,
            base::TaskPriority::USER_VISIBLE,
            base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN
        },
		base::BindOnce(&WalletSessionManager::set_session_cookie, base::Unretained(this))
    );
}

void WalletSessionManager::stop()
{
    Netboxglobal::Monitoring::ActivityWatcher::get_instance()->stop();
    cookie_listener_binding_.reset();
}

}
