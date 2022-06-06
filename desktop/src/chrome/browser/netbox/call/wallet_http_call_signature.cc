#include "chrome/browser/netbox/call/wallet_http_call_signature.h"

#include "base/base64.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "chrome/browser/netbox/call/netbox_error_codes.h"
#include "components/netboxglobal_verify/decode_public_key.h"
#include "components/netboxglobal_verify/netboxglobal_verify.h"
#include "crypto/signature_verifier.h"
#include "crypto/rsa_encrypt.h"
#include "net/base/load_flags.h"
#include "net/base/mime_util.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/cpp/simple_url_loader_stream_consumer.h"
#include "services/network/public/mojom/url_loader.mojom.h"

#define BOUNDARY_SEQUENCE "----**--yradnuoBgoLtrapitluMklaTelgooG--**----"

#define RPC_URL "http://127.0.0.1:28735"
#define RPC_URL_QA "http://127.0.0.1:28755"

#define SIGNED_API_URL "https://api.netbox.global"
#define SIGNED_API_QA_URL "https://devapi.netbox.global"

#define API_EXPLORER_URL "https://wallet.netbox.global"
#define API_EXPLORER_QA_URL "https://devwallet.netbox.global"

#define API_NOTIFICATION_URL "https://notifications.netbox.global"
#define API_NOTIFICATION_QA_URL "https://devnotifications.netbox.global"

#define API_BRIDGE_URL "https://bridge.netbox.global"
#define API_BRIDGE_QA_URL "https://devbridge.netbox.global"

#define ACCOUNT_URL "https://account.netbox.global"
#define ACCOUNT_QA_URL "https://devaccount.netbox.global"


namespace Netboxglobal
{


WalletHttpCallSignature::WalletHttpCallSignature(WalletHttpCallType type)
{
    type_ = type;
    params_ = base::Value(base::Value::Type::DICTIONARY);
}

WalletHttpCallSignature::~WalletHttpCallSignature()
{
}

WalletHttpCallSignature::WalletHttpCallSignature(WalletHttpCallSignature&& that)
{
    InternalMove(std::move(that));
}

WalletHttpCallSignature& WalletHttpCallSignature::operator=(WalletHttpCallSignature&& that)
{
    InternalMove(std::move(that));

    return *this;
}

void WalletHttpCallSignature::InternalMove(WalletHttpCallSignature&& that)
{
    type_               = that.type_;
    method_name_        = that.method_name_;
    event_name_         = that.event_name_;
    rpc_token_          = that.rpc_token_;
    params_             = std::move(that.params_);
    extra_data_         = std::move(that.extra_data_);
    tab_handler_        = that.tab_handler_;
    external_request_   = std::move(that.external_request_);
}

WalletHttpCallType WalletHttpCallSignature::get_type()
{
    return type_;
}

void WalletHttpCallSignature::set_method_name(const std::string method_name)
{
    method_name_ = method_name;
}

std::string WalletHttpCallSignature::get_method_name()
{
    return method_name_;
}

std::string WalletHttpCallSignature::get_event_name()
{
    return event_name_;
}

void WalletHttpCallSignature::set_event_name(const std::string event_name)
{
    event_name_ = event_name;
}

void WalletHttpCallSignature::set_params(base::Value params)
{
    params_ = std::move(params);
}

bool WalletHttpCallSignature::has_param(const std::string& param_name)
{
    if (params_.FindKey(param_name))
    {
        return true;
    }

    return false;
}

void WalletHttpCallSignature::append_param(const std::string& param_name, base::Value param_value)
{
    params_.SetKey(param_name, std::move(param_value));
}

base::Value& WalletHttpCallSignature::get_params()
{
    return params_;
}

void WalletHttpCallSignature::set_rpc_token(const std::string token)
{
    rpc_token_ = token;
}

std::string WalletHttpCallSignature::get_rpc_token()
{
    return rpc_token_;
}

void WalletHttpCallSignature::set_extra_data(base::Value extra_data)
{
    extra_data_ = std::move(extra_data);
}

base::Value& WalletHttpCallSignature::get_extra_data()
{
    return extra_data_;
}

void WalletHttpCallSignature::set_qa(bool is_qa)
{
    is_qa_ = is_qa;
}

void WalletHttpCallSignature::set_ui_handler(IWalletTabHandler* handler)
{
    tab_handler_ = handler;
}

IWalletTabHandler* WalletHttpCallSignature::get_ui_handler()
{
    return tab_handler_;
}

void WalletHttpCallSignature::set_external_signature(std::unique_ptr<WalletHttpCallSignature> external_request)
{
    external_request_ = std::move(external_request);
}

std::unique_ptr<WalletHttpCallSignature> WalletHttpCallSignature::get_external_signature()
{
    return std::move(external_request_);
}

bool WalletHttpCallSignature::has_external_signature()
{
    return nullptr != external_request_.get();
}

bool WalletHttpCallSignature::is_valid_to_call()
{
    // TODO, check methods against allowed_list
        // "add_dapp",
        // "add_dapp_image",
        // "buy",
        // "buy_status",
        // "delete_dapp",
        // "edit_dapp",
        // "get_dapp",
        // "get_dapp_config",
        // "get_dapp_thumbnails",
        // "get_dapps_info",
        // "get_email_by_first_address",
        // "get_system_addresses",
        // "get_system_features",
        // "lottery_address",
        // "lottery_last_pending_block",
        // "stake",
        // "staking_status",
        // "unstake",
        // "vote_dapp_against",
        // "vote_dapp_against_revert",
        // "vote_dapp_for",
        // "vote_dapp_for_revert",
        // "wallet_images"

    return true;
}

std::vector<std::string> encrypt_data(const std::string &data)
{
    std::string public_key = decode_public_key();
    if (public_key.empty())
    {
        return {};
    }

    std::vector<std::vector<uint8_t>> enc_data;
    if (false == crypto::RSAEncrypt(base::as_bytes(base::make_span(public_key)),

                                    base::as_bytes(base::make_span(data)),
                                    enc_data))
    {
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

std::string WalletHttpCallSignature::clue_get_params(std::string exclude_name)
{
	if (!params_.is_dict() || params_.DictEmpty())
	{
		return "";
	}

	std::string result;


    std::string piece;

    for (auto it : params_.DictItems())
    {
        if (exclude_name == it.first)
        {
            continue;
        }

        if (it.second.is_string())
        {
            result = result + piece + it.first + "=" + it.second.GetString();
        }
        else if (it.second.is_int())
        {
            result = result + piece + it.first + "=" + std::to_string(it.second.GetInt());
        }
        else{
            LOG(ERROR) << "missing param type " << it.first;
            break;
        }

        piece = "&";
    }

    return result;
}

std::unique_ptr<network::ResourceRequest> WalletHttpCallSignature::process_request_headers()
{
    std::unique_ptr<network::ResourceRequest> resource_request = std::make_unique<network::ResourceRequest>();

    if (WalletHttpCallType::API_SIGNED == get_type())
    {
        if (is_qa_)
        {
            resource_request->url = GURL(SIGNED_API_QA_URL "/data");
        }
        else
        {
            resource_request->url = GURL(SIGNED_API_URL "/data");
        }

        resource_request->credentials_mode = network::mojom::CredentialsMode::kInclude;
        resource_request->method = "POST";
        resource_request->site_for_cookies = net::SiteForCookies::FromUrl(GURL("https://netbox.global"));
    }
    else if (WalletHttpCallType::API_SIGNED_SIMPLE == get_type())
    {
        if (is_qa_)
        {
            resource_request->url = GURL(std::string(SIGNED_API_QA_URL "/") + get_method_name());
        }
        else
        {
            resource_request->url = GURL(std::string(SIGNED_API_URL "/") + get_method_name());
        }

        resource_request->credentials_mode = network::mojom::CredentialsMode::kInclude;
        resource_request->method = "GET";
        resource_request->site_for_cookies = net::SiteForCookies::FromUrl(GURL("https://netbox.global"));
    }
    else if (WalletHttpCallType::API_ACCOUNT == get_type())
    {
        if (is_qa_)
        {
            resource_request->url = GURL(std::string(ACCOUNT_QA_URL "/") + get_method_name());
        }
        else
        {
            resource_request->url = GURL(std::string(ACCOUNT_URL "/") + get_method_name());
        }

        resource_request->credentials_mode = network::mojom::CredentialsMode::kInclude;
        resource_request->method = "POST";
        resource_request->site_for_cookies = net::SiteForCookies::FromUrl(GURL("https://netbox.global"));
    }
    else if (WalletHttpCallType::RPC_JSON == get_type() || WalletHttpCallType::RPC_RAW == get_type())
    {
        if (is_qa_)
        {
            resource_request->url = GURL(RPC_URL_QA);
        }
        else
        {
            resource_request->url = GURL(RPC_URL);
        }
        resource_request->method = "POST";
        resource_request->headers.SetHeader("Authorization", std::string("Basic ") + get_rpc_token());
        resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
    }
    else if (WalletHttpCallType::API_EXPLORER == get_type())
    {
        std::string url = API_EXPLORER_URL;

        if (is_qa_)
        {
            url = API_EXPLORER_QA_URL;
        }

        url = url + "/api/v1/" + method_name_;

        if ("w/create" != method_name_
         && "tx/send" != method_name_)
        {
            const std::string* first_address = params_.FindStringKey("address");
            if (first_address)
            {
                url = url + "/" + *first_address;
            }
        }

        if ("w/create" != method_name_
         && "tx/send" != method_name_
         && params_.is_dict()
         && !params_.DictEmpty())
        {
            url = url + "?" + clue_get_params("address");
        }

        resource_request->url           = GURL(url);
        if ("w/create" == method_name_ || "tx/send" == method_name_ )
        {
            resource_request->method        = "POST";
        }
        else
        {
            resource_request->method        = "GET";
        }
        resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
    }
    else if (WalletHttpCallType::API_NOTIFICATION == get_type())
    {
        std::string url = API_NOTIFICATION_URL;

        if (is_qa_)
        {
            url = API_NOTIFICATION_QA_URL;
        }

        url = url + "/api/v1/" + method_name_ ;

        const std::string* first_address = params_.FindStringKey("address");
        if (first_address)
        {
            url = url + "/" + *first_address;
        }

        resource_request->url           = GURL(url);
        resource_request->method        = "POST";
        resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
    }
    else if (WalletHttpCallType::API_BRIDGE == get_type())
    {
        std::string url = API_BRIDGE_URL;

        if (is_qa_)
        {
            url = API_BRIDGE_QA_URL;
        }

        url = url + "/api/v1/" + method_name_ ;

        resource_request->url           = GURL(url);
        resource_request->method        = "GET";
        resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
    }
	else if (WalletHttpCallType::URL == get_type())
	{
		resource_request->url = GURL(get_method_name());
		resource_request->method = "GET";
		resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
	}
    else
    {
        DCHECK(false);
    }

    return resource_request;
}

void WalletHttpCallSignature::process_request_body(network::SimpleURLLoader* sender)
{
    if (WalletHttpCallType::RPC_JSON == get_type())
    {
        base::DictionaryValue dict;
        dict.SetKey("jsonrpc", base::Value("1.0"));
        dict.SetKey("method", base::Value(method_name_));
        dict.SetKey("params", std::move(params_));

        std::string json;
        base::JSONWriter::Write(dict, &json);

        sender->AttachStringForUpload(json, "text/plain");

        return;
    }

	if (WalletHttpCallType::RPC_RAW == get_type())
	{
		std::string json = "{\"jsonrpc\":\"1.0\",\"method\":\"" + method_name_ + "\",\"params\":[" + params_.GetString() + "]}";
		sender->AttachStringForUpload(json, "text/plain");

		return;
	}

    if (WalletHttpCallType::API_SIGNED == get_type())
    {
        // get json from params
        params_.SetKey("type", base::Value(method_name_));

        std::string data_json;
        base::JSONWriter::Write(params_, &data_json);

        // ecrypt
        std::string multipart_data;
        for (const std::string &b : encrypt_data(data_json))
        {
            net::AddMultipartValueForUpload("data[]", b, BOUNDARY_SEQUENCE, "", &multipart_data);
        }

        sender->AttachStringForUpload(
                multipart_data,
                "multipart/form-data; boundary=" BOUNDARY_SEQUENCE);

        return;
    }

    if (WalletHttpCallType::API_SIGNED_SIMPLE == get_type())
    {
        return;
    }

	if (WalletHttpCallType::API_ACCOUNT == get_type())
	{
        return;
    }

    if (WalletHttpCallType::API_EXPLORER == get_type())
    {
        if (("tx/send" == method_name_ || "w/create" == method_name_) && params_.is_dict())
        {
            std::string data_json;
            base::JSONWriter::Write(params_, &data_json);

            sender->AttachStringForUpload(data_json, "application/json");
        }

        return;
    }

    if (WalletHttpCallType::API_NOTIFICATION == get_type())
    {
        if (params_.is_dict())
        {
            std::string data_json;
            base::JSONWriter::Write(params_, &data_json);

            sender->AttachStringForUpload(data_json, "application/json");
        }

        return;
    }

    if (WalletHttpCallType::API_BRIDGE == get_type())
    {
        if (params_.is_dict())
        {
            std::string data_json;
            base::JSONWriter::Write(params_, &data_json);

            sender->AttachStringForUpload(data_json, "application/json");
        }

        return;
    }

	if (WalletHttpCallType::URL == get_type())
	{
		return;
	}

    DCHECK(false);
}

base::Value WalletHttpCallSignature::post_process_helper(base::Value dict, base::Value error_value)
{
    if (!dict.FindKey("error"))
    {
        dict.SetKey("error", std::move(error_value));
    }

    return dict;
}

base::Value  WalletHttpCallSignature::post_process(base::Value value, int32_t http_code)
{
    if (WalletHttpCallType::API_SIGNED == get_type()
     || WalletHttpCallType::API_SIGNED_SIMPLE == get_type())
    {
        if (401 == http_code)
        {
            base::Value result(base::Value::Type::DICTIONARY);
            result.SetKey("error", base::Value(401));

            return result;
        }

        if (!value.is_dict())
        {
            base::Value result(base::Value::Type::DICTIONARY);
            result.SetKey("error", base::Value(WR_ERROR_RESPONSE_NOT_JSON));

            return result;
        }

        absl::optional<bool> success = value.FindBoolKey("success");
        if (absl::nullopt == success)
        {
            return post_process_helper(std::move(value), base::Value(WR_ERROR_RESPONSE_FALSE_RESULT));
        }

        base::Value* data = value.FindKey("data");
        if (!data || !data->is_dict())
        {
            return post_process_helper(std::move(value), base::Value(WR_ERROR_RESPONSE_FALSE_RESULT));
        }

        if (false == *success)
        {
            return post_process_helper(data->Clone(), base::Value(WR_ERROR_RESPONSE_FALSE_RESULT));
        }

        return data->Clone();
    }

    if (WalletHttpCallType::RPC_JSON == get_type() || WalletHttpCallType::RPC_RAW == get_type())
    {
        if (!value.is_dict())
        {
            base::Value result(base::Value::Type::DICTIONARY);
            result.SetKey("error", base::Value(WR_ERROR_RESPONSE_NOT_JSON));

            return result;
        }

		auto error = value.ExtractKey("error");
		if (error && !error->is_none())
		{
			base::Value *error_text = error->FindKey("message");
			if (error_text && error_text->is_string())
			{
				value.SetKey("errortext", error_text->Clone());
			}
			else
			{
				value.SetKey("errortext", base::Value(""));
			}

			base::Value *error_code = error->FindKey("code");
			if (nullptr != error_code)
			{
				value.SetKey("error", error_code->Clone());
			}
			else
			{
				value.SetKey("error", base::Value("Can't obtain error number"));
			}

			return value;
		}

		auto result = value.ExtractKey("result");
		if (result)
		{
			if (result->is_dict())
			{
				value = result->Clone();
			}
			else
			{
				value.SetKey("result", result->Clone());
			}
		}

        return value;
    }

    // EXPLORER
    if (WalletHttpCallType::API_EXPLORER == get_type()
    || WalletHttpCallType::API_NOTIFICATION == get_type()
    || WalletHttpCallType::API_BRIDGE == get_type())
    {
        if (!value.is_dict())
        {
            base::Value result(base::Value::Type::DICTIONARY);
            result.SetKey("error", base::Value(WR_ERROR_RESPONSE_NOT_JSON));
            result.SetKey("errortext", base::Value(http_code));

            return result;
        }

        if (!value.FindKey("data") && !value.FindKey("error"))
        {
            value.SetKey("error", base::Value(http_code));
            return value;
        }


        return value;
    }

    DCHECK(false);
    return value;
}

}