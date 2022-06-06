#include "chrome/browser/netbox/session_manager/session_manager.h"

#include <regex>

#include <jni.h>
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/base64.h"
#include "base/guid.h"
#include "base/hash/md5.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/sequence_checker.h"
#include "chrome/android/chrome_jni_headers/NetboxHelper_jni.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/wallet_manager/wallet_manager.h"
#include "components/netboxglobal_call/netbox_error_codes.h"
#include "components/netboxglobal_verify/netboxglobal_verify.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/storage_partition.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_options.h"
#include "net/cookies/cookie_store.h"
#include "net/cookies/cookie_util.h"


#include "components/os_crypt/os_crypt.h"
#include "base/json/json_writer.h" // remove netboxtodo

namespace Netboxglobal
{

static std::string get_cookie_url()
{
    return "https://netbox.global";
}



WalletSessionManager::WalletSessionManager(bool is_qa): is_qa_(is_qa){}

WalletSessionManager::~WalletSessionManager()
{
    stop();

    machine_id_ = "";
    guid_ = "";
    guid_checksum_ = "";
}

void WalletSessionManager::stop()
{
    cookie_listener_binding_.reset();
}

network::mojom::CookieManager* WalletSessionManager::get_cookie_manager()
{

    Profile* profile = ProfileManager::GetLastUsedProfile();
    if (!profile)
    {
        return nullptr;
    }

    return content::BrowserContext::GetDefaultStoragePartition(profile)->GetCookieManagerForBrowserProcess();
}

std::unique_ptr<net::CanonicalCookie> get_cookie_helper(const std::string& url, const std::string& domain, const std::string& name, const std::string& value)
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

void WalletSessionManager::set_state(int state)
{
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    g_browser_process->wallet_manager()->set_session_state(state);
}

std::string WalletSessionManager::get_cookie_token_name() const
{
    return is_qa_ ? "devtoken" : "token";
}

std::string WalletSessionManager::get_machine_id()
{
    return machine_id_;
}

bool WalletSessionManager::is_emulator()
{
    return is_emulator_;
}

std::string WalletSessionManager::get_session_guid()
{
    return session_;
}

void WalletSessionManager::start(std::string referrer)
{
    referrer_ = referrer;

    obtain_information();

    set_session_cookie();

}

void WalletSessionManager::obtain_information()
{
    JNIEnv* env = base::android::AttachCurrentThread();
    const base::android::ScopedJavaLocalRef<jstring> info = Java_NetboxHelper_GetDeviceId(env);
    machine_id_ = base::android::ConvertJavaStringToUTF8(env, info);

    is_emulator_ = Java_NetboxHelper_isEmulator(env);

    session_ = regex_replace(base::ToUpperASCII(base::GenerateGUID()), std::regex("-"), "");
}

void WalletSessionManager::set_session_cookie()
{
    std::unique_ptr<net::CanonicalCookie> cookie = get_cookie_helper(
        get_cookie_url(),
        ".netbox.global",
        "session",
        session_
    );

    if (!cookie.get())
    {
        LOG(WARNING) << L"session, failed to create cookie";
        set_state(WR_ERROR_SESSION_SESSION);
        return;
    }

    network::mojom::CookieManager* cookie_manager = get_cookie_manager();
    if (!cookie_manager)
    {
        LOG(WARNING) << L"session, failed to get cookie_manager";
        set_state(WR_ERROR_SESSION_SESSION);
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
        LOG(WARNING) << L"session, failed to set cookie";
        set_state(WR_ERROR_SESSION_SESSION);
		return;
    }

    base::PostTask(
	    FROM_HERE,
	    {content::BrowserThread::UI},
	    base::Bind(&WalletSessionManager::process_guid, base::Unretained(this))
    );
}

void WalletSessionManager::process_guid()
{
    network::mojom::CookieManager* cookie_manager = get_cookie_manager();
    if (!cookie_manager)
    {
        LOG(WARNING) << L"failed to get cookie_manager for session";
        set_state(WR_ERROR_SESSION_GUID);
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
        LOG(WARNING) << L"guid, not found, created request";
        return false;
    }

    if (true == guid_checksum_raw.empty())
    {
        LOG(WARNING) << L"guid, sign not found, created request";
        return false;
    }

    std::string guid_checksum_raw_decoded;
    base::Base64Decode(guid_checksum_raw, &guid_checksum_raw_decoded);

    if (!verify_signature(base::as_bytes(base::make_span(guid_checksum_raw_decoded)), base::as_bytes(base::make_span(guid_raw))))
    {
        LOG(WARNING) << L"guid, invalid checksum, created request";
        return false;
    }

    guid_ = guid_raw;

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
        base::Bind(&WalletSessionManager::send_new_guid_request, base::Unretained(this))
    );

}

void WalletSessionManager::send_new_guid_request()
{
    is_first_launch_ = true;

    base::Value params(base::Value::Type::DICTIONARY);
    params.SetKey("machine_id",     base::Value(machine_id_));
    params.SetKey("version",        base::Value("1"));
    params.SetKey("referral_id",    base::Value(referrer_));

    std::unique_ptr<Netboxglobal::WalletHttpCallSignature> signature(new Netboxglobal::WalletHttpCallSignature(Netboxglobal::WalletHttpCallType::API_SIGNED));
    signature->set_method_name("get_guid");
    signature->set_params(std::move(params));

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
            base::Bind(&WalletSessionManager::send_new_guid_request, base::Unretained(this)),
            base::TimeDelta::FromSeconds(guid_request_interval_seconds_));

        return;
    };

    std::string* guid_raw = data.FindStringKey("guid");
    std::string* checksum_raw_encoded = data.FindStringKey("checksum");

    if (!guid_raw || !checksum_raw_encoded)
    {
        set_state(WR_ERROR_SESSION_GUID);
        return;
    }

    guid_ = std::string(guid_raw->c_str());
    guid_checksum_ = std::string(checksum_raw_encoded->c_str());

    std::string checksum_raw_decoded;
    base::Base64Decode(guid_checksum_, &checksum_raw_decoded);

    if (!verify_signature(base::as_bytes(base::make_span(checksum_raw_decoded)), base::as_bytes(base::make_span(guid_))))
    {
        guid_ = "";
        guid_checksum_ = "";

        set_state(WR_ERROR_SESSION_GUID);
        return;
    }

    set_data_to_cookie("browserguid", guid_, base::BindOnce(&WalletSessionManager::on_set_guid_cookie_result, base::Unretained(this)));
}

void WalletSessionManager::set_data_to_cookie(const std::string &variable,
                                       const std::string &value,
                                       network::mojom::CookieManager::SetCanonicalCookieCallback &&callback)
{
    network::mojom::CookieManager* cookie_manager = get_cookie_manager();
    if (!cookie_manager)
    {
        LOG(WARNING) << L"failed to get cookie_manager for guid";
        set_state(WR_ERROR_SESSION_GUID);
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
        LOG(WARNING) << L"failed to create GUID cookie";
        set_state(WR_ERROR_SESSION_GUID);
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
        set_data_to_cookie("browserguidsign", guid_checksum_, base::BindOnce(&WalletSessionManager::on_set_guid_sign_cookie_result, base::Unretained(this)));
    }
    else
    {
        LOG(WARNING) << L"failed to set GUID cookie";
        set_state(WR_ERROR_SESSION_GUID);
    }
}

void WalletSessionManager::on_set_guid_sign_cookie_result(net::CookieAccessResult set_cookie_result)
{
    if (set_cookie_result.status.IsInclude())
    {
        io_get_auth_cookie();
    }
    else
    {
        LOG(WARNING) << L"failed to set GUID checksum cookie";
        set_state(WR_ERROR_SESSION_GUID);
    }
}

void WalletSessionManager::io_get_auth_cookie()
{
    g_browser_process->wallet_manager()->set_guid(guid_);

    network::mojom::CookieManager* cookie_manager = get_cookie_manager();
    if (!cookie_manager)
    {
        LOG(WARNING) << L"failed to get cookie_manager for get auth";
        set_state(WR_ERROR_COOKIE_CONFIGURE);
        return;
    }

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

            LOG(WARNING) << L"start, " << cookie_token_name << " cookie found";
        }
    }

    g_browser_process->wallet_manager()->set_auth(has_auth_cookie);

    create_auth_cookie_subscription();
}

void WalletSessionManager::create_auth_cookie_subscription()
{
    network::mojom::CookieManager* cookie_manager = get_cookie_manager();
    if (!cookie_manager)
    {
        LOG(WARNING) << "cookie change handler, configure failed";
        set_state(WR_ERROR_COOKIE_CONFIGURE);

        return;
    }

    cookie_manager->AddCookieChangeListener(
        GURL(get_cookie_url()),
        get_cookie_token_name(),
        cookie_listener_binding_.BindNewPipeAndPassRemote());

    set_state(WALLET_SESSION_OK);
}

void WalletSessionManager::OnCookieChange(const net::CookieChangeInfo& change)
{
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    LOG(WARNING) << "cookie changed " << change.cookie.Name() << " "
                                                << change.cookie.Domain() << " "
                                                << change.cookie.Path() << " "
                                                << change.cookie.IsExpired(base::Time::Now())
                                                << " cause:" << static_cast<int32_t>(change.cause);

    if (change.cookie.IsExpired(base::Time::Now())
       || (net::CookieChangeCause::INSERTED != change.cause && net::CookieChangeCause::OVERWRITE != change.cause)
    )
    {
        g_browser_process->wallet_manager()->notify_auth(false);
    }
    else
    {
        g_browser_process->wallet_manager()->notify_auth(true);
    }
}

}