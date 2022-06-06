#include "chrome/browser/wallet_manager/wallet_manager.h"

#include <jni.h>
#include "base/android/build_info.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/base64.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/sequence_checker.h"
#include "chrome/android/chrome_jni_headers/NetboxWalletHelper_jni.h"
#include "chrome/android/chrome_jni_headers/NetboxShare_jni.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/gcm/instance_id/instance_id_profile_service_factory.h"
#include "chrome/browser/netbox_activity/activity_watcher.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/transaction_service/transaction_service.h"
#include "chrome/common/chrome_paths.h"
#include "components/gcm_driver/instance_id/instance_id_driver.h"
#include "components/gcm_driver/instance_id/instance_id_profile_service.h"
#include "components/netboxglobal_call/netbox_error_codes.h"
#include "components/netboxglobal_call/wallet_http_call_signature.h"
#include "components/netboxglobal_call/wallet_request.h"
#include "components/os_crypt/os_crypt.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_thread.h"

#include "base/json/json_writer.h"

#define FEATURE_DOWNLOAD "download"

namespace Netboxglobal
{

WalletManager::WalletManager(){}

WalletManager::~WalletManager(){}

void WalletManager::start()
{
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    JNIEnv* env = base::android::AttachCurrentThread();
    is_qa_ = Java_NetboxWalletHelper_GetIsTestnet(env);

    const base::android::ScopedJavaLocalRef<jstring> referrer_raw = Java_NetboxWalletHelper_GetReferrer(env);

    referrer_ = base::android::ConvertJavaStringToUTF8(env, referrer_raw);

    LOG(WARNING) << "referrer " << referrer_;

    reload_wallet();
    reload_session_controller();
}

void WalletManager::stop()
{
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    ActivityWatcher::GetInstance()->ui_stop();

    session_controller_->stop();
    session_controller_.reset();

    handlers_.clear();
}

void WalletManager::process_results_to_js(IWalletTabHandler* handler, std::string event_name, base::Value results)
{
    if (!handler)
    {
        return;
    }

    if (handlers_.end() != handlers_.find(handler))
    {
        handler->OnTabCall(event_name, std::move(results));
    }
}

void WalletManager::process_results_to_js_broadcast(std::string event_name, base::Value results)
{
    for(auto handler_iterator = handlers_.begin(); handler_iterator != handlers_.end(); ++handler_iterator)
    {
        base::Value data(base::Value::Type::DICTIONARY);
        (*handler_iterator)->OnTabCall(event_name, results.Clone());
    }
}


void WalletManager::reload_wallet()
{
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    JNIEnv* env = base::android::AttachCurrentThread();
    const base::android::ScopedJavaLocalRef<jstring> xpub_java = Java_NetboxWalletHelper_GetXPub(env, is_qa_);
    const base::android::ScopedJavaLocalRef<jstring> first_address_java = Java_NetboxWalletHelper_GetFirstAddress(env, is_qa_);

    xpub_ = base::android::ConvertJavaStringToUTF8(env, xpub_java);
    first_address_ = base::android::ConvertJavaStringToUTF8(env, first_address_java);

    base::android::ScopedJavaLocalRef<jstring> feature_name = base::android::ConvertUTF8ToJavaString(env, FEATURE_DOWNLOAD);
    feature_export_ = Java_NetboxWalletHelper_GetFeature(env, feature_name);

    // token
    token_flags_ = Java_NetboxWalletHelper_GetTokenFlags(env, is_qa_);

    const base::android::ScopedJavaLocalRef<jstring> token_lang_java = Java_NetboxWalletHelper_GetTokenLang(env, is_qa_);
    token_lang_ = base::android::ConvertJavaStringToUTF8(env, token_lang_java);

    const base::android::ScopedJavaLocalRef<jstring> token_first_address_java = Java_NetboxWalletHelper_GetTokenFirstAddress(env, is_qa_);
    token_first_address_ = base::android::ConvertJavaStringToUTF8(env, token_first_address_java);

    // set first address
    ActivityWatcher::GetInstance()->ui_set_first_address(first_address_, is_qa_);
    g_browser_process->transaction_service()->ui_set_first_address(first_address_, is_qa_);
}

void WalletManager::reload_session_controller()
{
    // load session
    if (session_controller_.get())
    {
        session_controller_->stop();
    }
    session_controller_.reset(new WalletSessionManager(is_qa_));
    session_controller_->start(referrer_);
}

void WalletManager::set_session_state(int wallet_session_state)
{
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    wallet_session_state_ = wallet_session_state;
}

void WalletManager::set_guid(std::string guid)
{
    installation_guid_ = guid;

    ActivityWatcher::GetInstance()->ui_start(guid);
}

void WalletManager::set_auth(bool is_auth)
{
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    is_auth_ = is_auth;
}

std::string WalletManager::get_machine_id()
{
    if (session_controller_.get())
    {
        return session_controller_->get_machine_id();
    }

    return "";
}

bool WalletManager::is_emulator()
{
    if (session_controller_.get())
    {
        return session_controller_->is_emulator();
    }

    return true;
}

void WalletManager::notify_auth(bool is_auth)
{
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    is_auth_ = is_auth;

    process_results_to_js_broadcast("environment", base::Value(base::Value::Type::DICTIONARY));
}

void WalletManager::add_handler(IWalletTabHandler* handler)
{
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    handlers_.insert(handler);
}

void WalletManager::remove_handler(IWalletTabHandler* handler)
{
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    handlers_.erase(handler);
}

void WalletManager::request_local(std::unique_ptr<WalletHttpCallSignature> &&signature)
{
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    // check event_name
    std::string event_name = signature->get_event_name();
    if (event_name.empty())
    {
        return;
    }

    // store handler value
    IWalletTabHandler* handler = signature->get_ui_handler();

    // call method
    base::Value result;
    if("environment" == signature->get_method_name())               { result = local_is_auth(std::move(signature)); }
    else if("is_wallet" == signature->get_method_name())            { result = local_is_wallet(std::move(signature)); }
    else if("store_wallet" == signature->get_method_name())         { return local_set_wallet(std::move(signature)); }
    else if("get_wallet" == signature->get_method_name())           { result = local_get_wallet(std::move(signature)); }
    else if("get_wallet_seed" == signature->get_method_name())      { result = local_get_wallet_seed(std::move(signature)); }
    else if("debug" == signature->get_method_name())                { result = local_debug(std::move(signature)); }
    else if("toggle_blockchain" == signature->get_method_name())    { result = local_toggle(std::move(signature)); }
    else if("toggle_feature_download" == signature->get_method_name())    { result = local_toggle_feature_download(std::move(signature)); }
    else if("reset_wallet" == signature->get_method_name())         { result = local_reset_wallet(std::move(signature)); }
    else if("get_address_index" == signature->get_method_name())    { result = local_get_address_index(std::move(signature)); }
    else if("set_address_index" == signature->get_method_name())    { result = local_set_address_index(std::move(signature)); }
    else if("share" == signature->get_method_name())                { result = local_share(std::move(signature)); }
    else if("import_transactions" == signature->get_method_name())  { return local_import_transactions(std::move(signature)); }
    else if("check_transactions" == signature->get_method_name())  { return local_check_transactions(std::move(signature)); }
    else if("get_last_block_hash" == signature->get_method_name()) { return local_get_last_block_hash(std::move(signature)); }
    else if("desync" == signature->get_method_name())               { return local_desync(std::move(signature)); }
    else if("transactions" == signature->get_method_name())         { return local_transactions(std::move(signature)); }
    else if("set_notification_token" == signature->get_method_name())         { return set_notification_token(std::move(signature)); }
    else
    {
        signature.reset();
    }

    process_results_to_js(handler, event_name, std::move(result));
}

void WalletManager::request_server(std::unique_ptr<WalletHttpCallSignature> &&signature)
{
    if (!signature->has_param("first_address"))
    {
        signature->append_param("first_address", base::Value(first_address_));
    }

    if ("get_system_addresses" == signature->get_method_name() || "get_system_features" == signature->get_method_name())
    {
        signature->append_param("browser_version", base::Value(version_info::GetVersionNumber()));

        std::string android_version = base::android::BuildInfo::GetInstance()->package_version_code();
        if (android_version.length() > 2)
        {
            android_version = android_version.substr(0, android_version.length() - 2);
        }
        signature->append_param("extra_version", base::Value(android_version));
        signature->append_param("os", base::Value("android"));
    }

    if ("create_wallet" == signature->get_method_name())
    {
        signature->append_param("guid", base::Value(installation_guid_));
    }

    std::unique_ptr<WalletRequest> http_request = std::make_unique<WalletRequest>();
    WalletRequest* http_request_ptr = http_request.get();

    if (is_qa_)
    {
        signature->set_qa(true);
    }

    http_request->start(std::move(signature),
        base::BindOnce(&WalletManager::on_http_response, base::Unretained(this)));

    ui_requests_[http_request_ptr] = std::move(http_request);
}

void WalletManager::request_explorer(std::unique_ptr<WalletHttpCallSignature> &&signature)
{
    std::unique_ptr<WalletRequest> http_request = std::make_unique<WalletRequest>();
    WalletRequest* http_request_ptr = http_request.get();

    if (is_qa_)
    {
        signature->set_qa(true);
    }

    http_request->start(std::move(signature),
        base::BindOnce(&WalletManager::on_http_response, base::Unretained(this)));

    ui_requests_[http_request_ptr] = std::move(http_request);
}

void WalletManager::on_http_response(std::unique_ptr<WalletHttpCallSignature> signature, base::Value results, WalletRequest* http_request_ptr)
{
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

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

    if (WalletHttpCallType::API_SIGNED == signature->get_type() && results.is_dict())
    {
        base::Optional<int> error = results.FindIntKey("error");
        if (base::nullopt != error && 401 == *error)
        {
            notify_auth(false);
        }
    }

    IWalletTabHandler* handler  = signature->get_ui_handler();
    std::string event_name      = signature->get_event_name();

    process_results_to_js(handler, event_name, std::move(results));
}

base::Value WalletManager::local_is_auth(std::unique_ptr<WalletHttpCallSignature> &&)
{
    base::Value result(base::Value::Type::DICTIONARY);

    if (wallet_session_state_ == WALLET_SESSION_LOADING)
    {
        result.SetKey("is_loaded", base::Value(false));
        return result;
    }

    result.SetKey("is_loaded", base::Value(true));

    if (wallet_session_state_ != WALLET_SESSION_OK)
    {
        result.SetKey("error", base::Value(wallet_session_state_));
        return result;
    }

    if (feature_export_)
    {
        result.SetKey("feature_export", base::Value(true));
    }

    result.SetKey("is_auth", base::Value(is_auth_));
    result.SetKey("is_qa", base::Value(is_qa_));

    return result;
}

base::Value WalletManager::local_is_wallet(std::unique_ptr<WalletHttpCallSignature> &&)
{
    base::Value result(base::Value::Type::DICTIONARY);

    if (first_address_.empty())
    {
        result.SetKey("is_wallet", base::Value(false));
    }
    else
    {
        result.SetKey("is_wallet", base::Value(true));
        result.SetKey("xpub", base::Value(xpub_));
        result.SetKey("first_address", base::Value(first_address_));

        if (!token_first_address_.empty())
        {
            result.SetKey("token_flags", base::Value(token_flags_));
            result.SetKey("token_lang", base::Value(token_lang_));
            result.SetKey("token_first_address", base::Value(token_first_address_));
        }
    }

    return result;
}

void WalletManager::local_set_wallet(std::unique_ptr<WalletHttpCallSignature> &&signature)
{
    base::Value result(base::Value::Type::DICTIONARY);
    base::Value& params = signature->get_params();

    const std::string* seed         = params.FindStringKey("seed");
    const std::string* wallet58     = params.FindStringKey("wallet58");
    const std::string* xpub         = params.FindStringKey("xpub");
    const std::string* first_address = params.FindStringKey("first_address");
    const std::string* password     = params.FindStringKey("password");

    if (!seed || !wallet58 || !xpub || !first_address || !password)
    {
        result.SetKey("error", base::Value(WR_ERROR_WRONG_PARAMS));

        IWalletTabHandler* handler  = signature->get_ui_handler();
        std::string event_name      = signature->get_event_name();
        process_results_to_js(handler, event_name, std::move(result));

        return;
    }

    std::string encrypted_seed, encrypted_wallet;
    if (!::OSCrypt::EncryptStringNetbox(*seed, *password, &encrypted_seed)
        || !::OSCrypt::EncryptStringNetbox(*wallet58, *password, &encrypted_wallet))
    {
        result.SetKey("error", base::Value(WR_ERROR_ENCRYPTION_FAILED));

        IWalletTabHandler* handler  = signature->get_ui_handler();
        std::string event_name      = signature->get_event_name();
        process_results_to_js(handler, event_name, std::move(result));

        return;
    }

    std::string encrypted_seed_base64, encrypted_wallet_base64;
    base::Base64Encode(std::string({encrypted_seed.begin(), encrypted_seed.end()}), &encrypted_seed_base64);
    base::Base64Encode(std::string({encrypted_wallet.begin(), encrypted_wallet.end()}), &encrypted_wallet_base64);

    JNIEnv* env = base::android::AttachCurrentThread();
    base::android::ScopedJavaLocalRef<jstring> seed_java           = base::android::ConvertUTF8ToJavaString(env, encrypted_seed_base64);
    base::android::ScopedJavaLocalRef<jstring> wallet_java         = base::android::ConvertUTF8ToJavaString(env, encrypted_wallet_base64);
    base::android::ScopedJavaLocalRef<jstring> xpub_java           = base::android::ConvertUTF8ToJavaString(env, *xpub);
    base::android::ScopedJavaLocalRef<jstring> first_address_java  = base::android::ConvertUTF8ToJavaString(env, *first_address);

    if (!Java_NetboxWalletHelper_SetWallet(env, seed_java, wallet_java, xpub_java, first_address_java, is_qa_))
    {
        result.SetKey("error", base::Value(WR_ERROR_SET_WALLET_FAIL));

        IWalletTabHandler* handler  = signature->get_ui_handler();
        std::string event_name      = signature->get_event_name();
        process_results_to_js(handler, event_name, std::move(result));

        return;
    }

    if (!first_address_.empty())
    {
        return g_browser_process->transaction_service()->ui_remove_database(std::move(signature));
    }

    on_remove_database(std::move(signature));
}

void WalletManager::on_remove_database(std::unique_ptr<WalletHttpCallSignature> &&signature)
{
    reload_wallet();

    IWalletTabHandler* handler  = signature->get_ui_handler();
    std::string event_name      = signature->get_event_name();
    process_results_to_js(handler, event_name, base::Value(base::Value::Type::DICTIONARY));
}

base::Value WalletManager::local_get_wallet(std::unique_ptr<WalletHttpCallSignature> &&signature)
{
    base::Value result(base::Value::Type::DICTIONARY);
    base::Value& params = signature->get_params();
    const std::string* password     = params.FindStringKey("password");

    if (first_address_.empty())
    {
        result.SetKey("error", base::Value(WR_ERROR_GET_WALLET_FAIL));
        return result;
    }

    if (!password)
    {
        result.SetKey("error", base::Value(WR_ERROR_WRONG_PARAMS));
        return result;
    }

    // get wallet58
    JNIEnv* env = base::android::AttachCurrentThread();
    const base::android::ScopedJavaLocalRef<jstring> wallet58_java = Java_NetboxWalletHelper_GetWallet(env, is_qa_);
    std::string wallet58_base64 = base::android::ConvertJavaStringToUTF8(env, wallet58_java);

    std::string wallet58_encrypted;
    base::Base64Decode(wallet58_base64, &wallet58_encrypted);

    std::string wallet58;
    if(!::OSCrypt::DecryptStringNetbox(wallet58_encrypted, *password, &wallet58))
    {
        result.SetKey("error", base::Value(WR_ERROR_DECRYPTION_FAILED));
        return result;
    }

    result.SetKey("wallet58", base::Value(wallet58));

    return result;
}

base::Value WalletManager::local_reset_wallet(std::unique_ptr<WalletHttpCallSignature> &&signature)
{
    base::Value result(base::Value::Type::DICTIONARY);

    JNIEnv* env = base::android::AttachCurrentThread();

    if (!Java_NetboxWalletHelper_ResetWallet(env))
    {
        result.SetKey("error", base::Value(WR_ERROR_LOCAL_FUNCTION_ERROR));
        return result;
    }

    reload_wallet();

    return result;
}

base::Value WalletManager::local_set_address_index(std::unique_ptr<WalletHttpCallSignature> &&signature)
{
    base::Value result(base::Value::Type::DICTIONARY);

    base::Value& params = signature->get_params();
    const base::Optional<int> last_index = params.FindIntKey("last_index");

    if (base::nullopt == last_index)
    {
        result.SetKey("error", base::Value(WR_ERROR_WRONG_PARAMS));
        return result;
    }

    JNIEnv* env = base::android::AttachCurrentThread();
    if (!Java_NetboxWalletHelper_SetAddressIndex(env, *last_index, is_qa_))
    {
        result.SetKey("error", base::Value(WR_ERROR_LOCAL_FUNCTION_ERROR));
    }

    return result;
}

base::Value WalletManager::local_get_address_index(std::unique_ptr<WalletHttpCallSignature> &&signature)
{
    JNIEnv* env = base::android::AttachCurrentThread();
    int address_index = Java_NetboxWalletHelper_GetAddressIndex(env, is_qa_);

    base::Value result(base::Value::Type::DICTIONARY);
    result.SetKey("last_index", base::Value(address_index));

    return result;
}

base::Value WalletManager::local_share(std::unique_ptr<WalletHttpCallSignature> &&signature)
{
    base::Value& params = signature->get_params();

    const std::string* link_raw     = params.FindStringKey("link");
    std::string link;

    if (link_raw) {
        link = *link_raw;
    }

    std::string share_text = "";
    if (params.FindStringKey("share_text"))
    {
        share_text = *(params.FindStringKey("share_text"));
    }

    JNIEnv* env = base::android::AttachCurrentThread();
    if (false == Java_NetboxShare_Share(env,
            base::android::ConvertUTF8ToJavaString(env, link),
            base::android::ConvertUTF8ToJavaString(env, share_text)))
    {
        base::Value result(base::Value::Type::DICTIONARY);
        result.SetKey("error", base::Value(WR_ERROR_WRONG_PARAMS));

        return result;
    }

    return base::Value(base::Value::Type::DICTIONARY);
}

base::Value WalletManager::local_debug(std::unique_ptr<WalletHttpCallSignature> &&signature)
{
    base::Value result(base::Value::Type::DICTIONARY);
    base::Value debug(base::Value::Type::LIST);

    // get current blockchain
    if (is_qa_)
    {
        debug.Append(base::Value("blockchain is in testnet mode"));
    }
    else
    {
        debug.Append("blockchain is in live mode");
    }

    if (feature_export_)
    {
        debug.Append("export transaction is enabled");
    }
    else
    {
        debug.Append("export transaction is disabled");
    }

    // get first address
    debug.Append(base::Value("first address is " + first_address_));

    debug.Append(base::Value("version is " + std::string(base::android::BuildInfo::GetInstance()->package_version_code())));

    result.SetKey("debug", std::move(debug));

    return result;
}

base::Value WalletManager::local_toggle(std::unique_ptr<WalletHttpCallSignature> &&signature)
{
    wallet_session_state_ = WALLET_SESSION_LOADING;

    is_qa_ = !is_qa_;

    JNIEnv* env = base::android::AttachCurrentThread();
    Java_NetboxWalletHelper_SetIsTestnet(env, is_qa_);

    reload_wallet();
    reload_session_controller();

    // reload wallet tabs
    process_results_to_js_broadcast("reload", base::Value(base::Value::Type::DICTIONARY));

    return base::Value(base::Value::Type::DICTIONARY);
}

base::Value WalletManager::local_toggle_feature_download(std::unique_ptr<WalletHttpCallSignature> &&signature)
{
    feature_export_ = !feature_export_;

    JNIEnv* env = base::android::AttachCurrentThread();
    base::android::ScopedJavaLocalRef<jstring> feature_name = base::android::ConvertUTF8ToJavaString(env, FEATURE_DOWNLOAD);
    Java_NetboxWalletHelper_SetFeature(env, feature_name, feature_export_);

    return base::Value(base::Value::Type::DICTIONARY);
}


void WalletManager::local_get_last_block_hash(std::unique_ptr<WalletHttpCallSignature> &&signature)
{
    g_browser_process->transaction_service()->ui_get_last_block_hash(std::move(signature));
}


void WalletManager::local_import_transactions(std::unique_ptr<WalletHttpCallSignature> &&signature)
{
    base::Optional<int> height = signature->get_params().FindIntKey("height");
    std::string* block_hash = signature->get_params().FindStringKey("block_hash");

    if (base::nullopt != height && block_hash)
    {
        g_browser_process->transaction_service()->ui_import_transactions(std::move(signature), *height, *block_hash);
    }
}

void WalletManager::local_check_transactions(std::unique_ptr<WalletHttpCallSignature> &&signature)
{
    g_browser_process->transaction_service()->ui_check_transactions(std::move(signature));
}

void WalletManager::local_transactions(std::unique_ptr<WalletHttpCallSignature> &&signature)
{
    g_browser_process->transaction_service()->ui_get_transactions(std::move(signature));
}

void WalletManager::local_desync(std::unique_ptr<WalletHttpCallSignature> &&signature)
{
    g_browser_process->transaction_service()->ui_desync(std::move(signature));
}

base::Value WalletManager::local_get_wallet_seed(std::unique_ptr<WalletHttpCallSignature> &&signature)
{
    base::Value result(base::Value::Type::DICTIONARY);

    base::Value& params = signature->get_params();
    const std::string* password     = params.FindStringKey("password");
    if (!password)
    {
        result.SetKey("error", base::Value(WR_ERROR_WRONG_PARAMS));
        return result;
    }

    JNIEnv* env = base::android::AttachCurrentThread();
    const base::android::ScopedJavaLocalRef<jstring> seed_java = Java_NetboxWalletHelper_GetSeed(env, is_qa_);
    std::string seed_encrypted_base64 = base::android::ConvertJavaStringToUTF8(env, seed_java);

    std::string seed_encrypted;
    base::Base64Decode(seed_encrypted_base64, &seed_encrypted);

    std::string seed_decrypted;
    if(!::OSCrypt::DecryptStringNetbox(seed_encrypted, *password, &seed_decrypted))
    {
        result.SetKey("error", base::Value(WR_ERROR_DECRYPTION_FAILED));
        return result;
    }

    result.SetKey("seed", base::Value(seed_decrypted));

    return result;
}


void WalletManager::set_notification_token(std::unique_ptr<WalletHttpCallSignature> &&signature)
{
    base::Value& outside_params = signature->get_params();

    std::string* lang = outside_params.FindStringKey("lang");
    const base::Optional<int> flags = outside_params.FindIntKey("flags");
    if (!flags || !lang || first_address_.empty())
    {
        base::Value result(base::Value::Type::DICTIONARY);
        result.SetKey("error", base::Value(WR_ERROR_WRONG_PARAMS));

        IWalletTabHandler* handler  = signature->get_ui_handler();
        std::string event_name      = signature->get_event_name();

        process_results_to_js(handler, event_name, std::move(result));
    }

    std::string kTestAppID1 = "1:982899751369:android:b9cde6059eb34da05cd018s";
    // Returns the InstanceID that provides the Instance ID service for the given
    // application. The lifetime of the InstanceID will be managed by this class.
    // App IDs are arbitrary strings that typically look like "chrome.foo.bar".
    //virtual InstanceID* GetInstanceID(const std::string& app_id);

    std::string project_id_as_authorized_entity = "982899751369";
    // |authorized_entity|: identifies the entity that is authorized to access
    //                      resources associated with this Instance ID. It can be
    //                          another Instance ID or a numeric project ID.


    std::string somescope = "GCM";
        // |scope|: identifies authorized actions that the authorized entity can take.
    //          E.g. for sending GCM messages, "GCM" scope should be used.

    Profile* profile = ProfileManager::GetLastUsedProfile();
    instance_id::InstanceID* handler = instance_id::InstanceIDProfileServiceFactory::GetForProfile(profile)->driver()->GetInstanceID(kTestAppID1);

    handler->GetToken(
            project_id_as_authorized_entity,
            somescope,
            base::TimeDelta(), // unlimited live time
            {}, // flags
            base::BindOnce(&WalletManager::GetTokenCompleted,
                        base::Unretained(this), std::move(signature), *flags, *lang));
}

void WalletManager::GetTokenCompleted(std::unique_ptr<WalletHttpCallSignature> &&signature, int flags, const std::string lang, const std::string& token, instance_id::InstanceID::Result result)
{
    if (instance_id::InstanceID::SUCCESS == result)
    {
        set_notification_token_server(std::move(signature), token, flags, lang);
        return;

    }

    base::Value error(base::Value::Type::DICTIONARY);
    error.SetKey("error", base::Value("failed to get token"));

    IWalletTabHandler* handler  = signature->get_ui_handler();
    std::string event_name      = signature->get_event_name();

    process_results_to_js(handler, event_name, std::move(error));
}

void WalletManager::set_notification_token_server(std::unique_ptr<WalletHttpCallSignature> &&signature, std::string token, int flags, std::string lang)
{
    std::unique_ptr<WalletHttpCallSignature> call_signature = std::make_unique<WalletHttpCallSignature>(WalletHttpCallType::API_NOTIFICATION);
    call_signature->set_method_name("device/register");

    call_signature->set_external_signature(std::move(signature));

    base::Value params(base::Value::Type::DICTIONARY);
    params.SetStringKey("token", token);
    params.SetIntKey("flags", flags);
    params.SetStringKey("lang", lang);
    params.SetStringKey("address", first_address_);

    call_signature->set_params(std::move(params));
    call_signature->set_qa(is_qa_);

    // make call
    std::unique_ptr<WalletRequest> http_request = std::make_unique<WalletRequest>();
    WalletRequest* http_request_ptr = http_request.get();

    http_request->start(std::move(call_signature),
        base::BindOnce(&WalletManager::set_notification_token_response, base::Unretained(this), flags, lang, first_address_));

    ui_requests_[http_request_ptr] = std::move(http_request);

}

void WalletManager::set_notification_token_response(int flags, std::string lang, std::string first_address, std::unique_ptr<WalletHttpCallSignature> signature, base::Value results, WalletRequest* http_request_ptr)
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

    if (!results.FindKey("error"))
    {
        JNIEnv* env = base::android::AttachCurrentThread();
        base::android::ScopedJavaLocalRef<jstring> token_lang_java = base::android::ConvertUTF8ToJavaString(env, lang);
        base::android::ScopedJavaLocalRef<jstring> token_first_address_java = base::android::ConvertUTF8ToJavaString(env, first_address);

        if (Java_NetboxWalletHelper_SetTokenDetails(env, flags, token_lang_java, token_first_address_java, is_qa_))
        {
            token_flags_         = flags;
            token_lang_          = lang;
            token_first_address_ = first_address;
        }
    }

    std::unique_ptr<WalletHttpCallSignature> external_signature = signature->get_external_signature();

    if (external_signature.get())
    {
        IWalletTabHandler* handler  = external_signature->get_ui_handler();
        std::string event_name      = external_signature->get_event_name();

        process_results_to_js(handler, event_name, std::move(results));
    }
}

}