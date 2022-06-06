#include "chrome/browser/ui/webui/news/news.h"

#include "base/environment.h"
#include "base/json/json_reader.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/netbox_resources.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_store.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include <mutex>

#define LANG_COOKIE_URL "https://netbox.global"
#define LANG_COOKIE_NAME "newslang"

namespace
{

class MessageHandler;

class HTTPFetcher
{
public:
    explicit HTTPFetcher(MessageHandler *handler);
    int32_t get_http_code() const;
    bool send(const std::string& host, const std::string& url, const std::string& method_name, const std::string& selector);
    std::string method_name_;
    std::string selector_;
private:
    std::unique_ptr<network::SimpleURLLoader> sender_;
    MessageHandler *handler_ = nullptr;
    int32_t http_code_ = 0;
    void on_http_response_started(const GURL& final_url, const network::mojom::URLResponseHead& response_head);
    void on_http_response_body(std::unique_ptr<std::string> response);
};

class MessageHandler : public content::WebUIMessageHandler
{
public:
    MessageHandler();
    ~MessageHandler() override;

    void RegisterMessages() override;

    void OnUniversalResult(const std::string *data, HTTPFetcher *fetcher);
 private:
    void HandleGetUniversal(const base::ListValue *args);
    
    std::unique_ptr<net::CanonicalCookie> get_cookie_helper(const std::string& url, const std::string& domain, const std::string& name, const std::string& value);
    void on_set_lang_cookie(net::CanonicalCookie::CookieInclusionStatus);
    network::mojom::CookieManager* get_cookie_manager();
    void set_lang_cookie(const std::string& lang);
    
    void get_cookie();
    void on_get_lang_cookies_list(const net::CookieStatusList& cookie_list, const net::CookieStatusList&);
    
    std::string filter_lang(const std::string& lang);
    

    std::list<HTTPFetcher*> fetchers_;
    std::mutex fetchers_mutex_;

    std::string host_;

    base::WeakPtrFactory<MessageHandler> weak_ptr_factory_{this};
    DISALLOW_COPY_AND_ASSIGN(MessageHandler);
};

MessageHandler::MessageHandler()
{
    std::unique_ptr<base::Environment> env(base::Environment::Create());

    if (env->HasVar("NETBOXQA"))
    {
        host_ = "devapi.netbox.global";
    }
    else
    {
        host_ = "devapi.netbox.global";
    }
}

MessageHandler::~MessageHandler()
{
    for (HTTPFetcher *f : fetchers_)
        delete f;
}

void MessageHandler::RegisterMessages()
{
    web_ui()->RegisterMessageCallback("universal", base::BindRepeating(&MessageHandler::HandleGetUniversal, weak_ptr_factory_.GetWeakPtr()));
}

void MessageHandler::HandleGetUniversal(const base::ListValue *args)
{
    std::string method_name;
    if (args->GetString(0, &method_name))
    {
        if (method_name == "setlang")
        {
            std::string lang;
            if (args->GetString(1, &lang))
            {
                set_lang_cookie(lang);
            }
            return;
        }
        
        if (method_name == "getlang")
        {
            return get_cookie();
        }        
        
        std::lock_guard<std::mutex> grd(fetchers_mutex_);

        HTTPFetcher *fetcher = new HTTPFetcher(this);

        if (method_name == "news")
        {
            std::string lang;

            if (args->GetString(1, &lang))
            {                
                if (true == fetcher->send(host_, "https://" + host_ + "/news/" + lang + ".json", method_name, ""))
                {
                    fetchers_.push_back(fetcher);
                    return;
                }
            }
        }

        if (method_name == "image")
        {
            std::string url, selector;
            if (args->GetString(1, &url) && args->GetString(2, &selector))
            {
                if (true == fetcher->send(host_, url, method_name, selector))
                {
                    fetchers_.push_back(fetcher);
                    return;
                }
            }
        }
        
        delete fetcher;
    }
}

std::unique_ptr<net::CanonicalCookie> MessageHandler::get_cookie_helper(const std::string& url, const std::string& domain, const std::string& name, const std::string& value)
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
        net::CookiePriority::COOKIE_PRIORITY_HIGH);
}

network::mojom::CookieManager* MessageHandler::get_cookie_manager()
{

    Profile* profile = ProfileManager::GetLastUsedProfile();
    if (!profile)
    {        
        VLOG(NETBOX_LOG_LEVEL) << L"can't get profile";
        return nullptr;
    }

    return content::BrowserContext::GetDefaultStoragePartition(profile)->GetCookieManagerForBrowserProcess();
}

std::string MessageHandler::filter_lang(const std::string& lang)
{
    if (   "ar" == lang
        || "zh" == lang
        || "de" == lang
        || "en" == lang
        || "es" == lang
        || "it" == lang
        || "jp" == lang
        || "ko" == lang
        || "pt" == lang
        || "tr" == lang)
    {
      return lang;
    }
  
  return "en";
}

void MessageHandler::set_lang_cookie(const std::string& lang)
{
    std::unique_ptr<net::CanonicalCookie> cookie = get_cookie_helper(
        LANG_COOKIE_URL,
        ".netbox.global",
        "newslang",
        filter_lang(lang)
    );

    if (!cookie.get())
    {
        VLOG(NETBOX_LOG_LEVEL) << L"failed to create lang cookie";
        return;
    }
        
    network::mojom::CookieManager* cookie_manager = get_cookie_manager();
    if (!cookie_manager)
    {
        VLOG(NETBOX_LOG_LEVEL) << L"failed to get cookie_manager for lang";
        return;
    }
    
    net::CookieOptions options;
    options.set_include_httponly();
    cookie_manager->SetCanonicalCookie(*cookie.get(), "https", options, base::BindOnce(&MessageHandler::on_set_lang_cookie, base::Unretained(this)));

}

void MessageHandler::on_set_lang_cookie(net::CanonicalCookie::CookieInclusionStatus)
{
    base::Value event_name("setlang");
    base::DictionaryValue data;
    web_ui()->CallJavascriptFunctionUnsafe("netbox_send_event", std::move(event_name), std::move(data));
}

void MessageHandler::get_cookie()
{
    network::mojom::CookieManager* cookie_manager = get_cookie_manager();
    if (!cookie_manager)
    {
        VLOG(NETBOX_LOG_LEVEL) << L"failed to get cookie_manager for get lang";
        return;
    }

    net::CookieOptions options;
    options.set_include_httponly();
    options.set_return_excluded_cookies();

    cookie_manager->GetCookieList(
        GURL(LANG_COOKIE_URL), 
        options,
        base::BindOnce(&MessageHandler::on_get_lang_cookies_list, base::Unretained(this))
    );
}
    
void MessageHandler::on_get_lang_cookies_list(const net::CookieStatusList& cookie_list, const net::CookieStatusList&)
{
    std::string lang = "en";
    
    for (const net::CookieWithStatus &c : cookie_list)
    {
        if (c.cookie.Name() == LANG_COOKIE_NAME &&  false == c.cookie.IsExpired(base::Time::Now()))
        {
            lang = filter_lang(c.cookie.Value());
        }
    }    
    
    base::Value event_name("getlang");
    base::DictionaryValue data;
    data.SetKey("success",  base::Value(true));
    data.SetKey("lang",     base::Value(lang));
    web_ui()->CallJavascriptFunctionUnsafe("netbox_send_event", std::move(event_name), std::move(data));
}

void MessageHandler::OnUniversalResult(const std::string *http_data, HTTPFetcher *fetcher)
{
    {

        std::lock_guard<std::mutex> grd(fetchers_mutex_);

        base::DictionaryValue data;

        data.SetKey("success", base::Value(false));

        if (http_data != nullptr)
        {

            if ("news" == fetcher->method_name_)
            {
                base::Optional<base::Value> json = base::JSONReader::Read(*http_data);
                base::DictionaryValue *outdata;

                if (json && json->GetAsDictionary(&outdata) && outdata)
                {
                    data.SetKey("data", std::move(*outdata));
                    data.SetKey("success", base::Value(true));
                }
            }

            if ("image" == fetcher->method_name_)
            {
                data.SetKey("image",    base::Value(*http_data));
                data.SetKey("selector", base::Value(fetcher->selector_));
                data.SetKey("success",  base::Value(true));
            }
        }

        base::Value event_name(fetcher->method_name_);
        web_ui()->CallJavascriptFunctionUnsafe("netbox_send_event", std::move(event_name), std::move(data));


        auto it = std::find(fetchers_.begin(), fetchers_.end(), fetcher);

        if (fetchers_.end() != it)
        {
            delete *it;
            fetchers_.erase(it);
        }

    }
}

HTTPFetcher::HTTPFetcher(MessageHandler *handler)
    : handler_(handler)
{}

bool HTTPFetcher::send(const std::string& host, const std::string& url, const std::string& method_name, const std::string& selector)
{
    selector_    = selector;
    method_name_ = method_name;

    Profile *profile = ProfileManager::GetLastUsedProfile();

    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory = content::BrowserContext::GetDefaultStoragePartition(profile)->GetURLLoaderFactoryForBrowserProcess();

    auto resource_request = std::make_unique<network::ResourceRequest>();

    resource_request->url = GURL(url);
    resource_request->method = "GET";
    resource_request->site_for_cookies = net::SiteForCookies::FromUrl(GURL("https://" + host));

    sender_ = network::SimpleURLLoader::Create(std::move(resource_request), TRAFFIC_ANNOTATION_FOR_TESTS);

    if (nullptr == sender_)
    {
        VLOG(NETBOX_LOG_LEVEL) << L"news, failed to create sender, " << url;
        return false;
    }

    VLOG(NETBOX_LOG_LEVEL) << "news, success, " << url;

    sender_->SetAllowHttpErrorResults(true);
    sender_->SetOnResponseStartedCallback(base::BindOnce(&HTTPFetcher::on_http_response_started, base::Unretained(this)));
    sender_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(url_loader_factory.get(), base::BindOnce(&HTTPFetcher::on_http_response_body, base::Unretained(this)));

    return true;
}

int32_t HTTPFetcher::get_http_code() const
{
    return http_code_;
}

void HTTPFetcher::on_http_response_started(const GURL& final_url, const network::mojom::URLResponseHead& response_head)
{
    if (nullptr != response_head.headers)
    {
        http_code_ = response_head.headers->response_code();
    }
}

void HTTPFetcher::on_http_response_body(std::unique_ptr<std::string> response)
{
    if (nullptr == response)
    {
        VLOG(NETBOX_LOG_LEVEL) << L"network error for HTTP news request " << static_cast<HTTPFetcher*>(this);
        handler_->OnUniversalResult(nullptr, this);
    }
    else
    {
        VLOG(NETBOX_LOG_LEVEL) << "HTTP news " << static_cast<HTTPFetcher*>(this) << " request return " << http_code_;
        handler_->OnUniversalResult(response.get(), this);
    }
}

}

NewsUI::NewsUI(content::WebUI* web_ui)
    : WebUIController(web_ui)
{
    Profile* profile = Profile::FromWebUI(web_ui);

    web_ui->AddMessageHandler(std::make_unique<::MessageHandler>());

    content::WebUIDataSource* source = content::WebUIDataSource::Create(chrome::kChromeUINewsHost);

    source->AddResourcePath("img/528x352.png", IDR_IMAGE_BLANK);
    source->AddResourcePath("img/cointelegraph.svg", IDR_IMAGE_LOGO);
    source->AddResourcePath("js/news.js", IDR_NEWS_JS);
    source->AddResourcePath("css/news.css", IDR_NEWS_CSS);
    source->SetDefaultResource(IDR_NEWS_HTML_MAIN);

    content::WebUIDataSource::Add(profile, source);
    content::URLDataSource::Add(profile, std::make_unique<ThemeSource>(profile));
}