
#include "chrome/browser/netbox/call/wallet_request.h"

#include "base/base64.h"
#include "base/hash/md5.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/netbox/call/netbox_error_codes.h"
#include "content/public/browser/storage_partition.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/base/load_flags.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace Netboxglobal
{

WalletRequest::WalletRequest()
{
}

WalletRequest::~WalletRequest()
{
}

void WalletRequest::start(std::unique_ptr<WalletHttpCallSignature> signature, base::OnceCallback<void(std::unique_ptr<WalletHttpCallSignature>, base::Value, WalletRequest*)> callback)
{
    signature_ = std::move(signature);
    callback_ = std::move(callback);

    if (!signature_->is_valid_to_call())
    {
        base::Value result(base::Value::Type::DICTIONARY);
        result.SetKey("error", base::Value(WR_ERROR_CALL_NOT_ALLOWED));

        std::move(callback_).Run(std::move(signature_), std::move(result), this);
        return;
    }

    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory =
        ProfileManager::GetLastUsedProfile()->GetDefaultStoragePartition()->GetURLLoaderFactoryForBrowserProcess();
        //content::BrowserContext::GetDefaultStoragePartition(ProfileManager::GetLastUsedProfile())->GetURLLoaderFactoryForBrowserProcess();

    sender_ = network::SimpleURLLoader::Create(signature_->process_request_headers(), TRAFFIC_ANNOTATION_FOR_TESTS);

    if (!sender_ || !url_loader_factory)
    {
        base::Value result(base::Value::Type::DICTIONARY);
        result.SetKey("error", base::Value(WR_ERROR_INTERNAL));
        std::move(callback_).Run(std::move(signature_), base::Value(), this);
        return;
    }

    sender_->SetAllowHttpErrorResults(true);
    sender_->SetOnResponseStartedCallback(base::BindOnce(&WalletRequest::on_http_response_started, base::Unretained(this)));
    signature_->process_request_body(sender_.get());

    sender_->DownloadAsStream(url_loader_factory.get(), this);
}


void WalletRequest::OnDataReceived(base::StringPiece string_piece, base::OnceClosure resume)
{
    data_ << string_piece;
    std::move(resume).Run();
}

void WalletRequest::on_http_response_started(const GURL& final_url, const network::mojom::URLResponseHead& response_head)
{
    if (nullptr != response_head.headers)
    {
        http_code_ = response_head.headers->response_code();
    }
}

void WalletRequest::OnComplete(bool success)
{
	if (!success && WalletHttpCallType::RPC_JSON == signature_->get_type())
	{
		base::Value result(base::Value::Type::DICTIONARY);
		result.SetKey("netboxrestart", base::Value(true));
		result.SetKey("error", base::Value(WR_ERROR_HTTP_CODE_EMPTY));
        std::move(callback_).Run(std::move(signature_), std::move(result), this);
        return;
	}

    if (!success && http_code_ != 404 && http_code_ != 422)
    {
        base::Value result(base::Value::Type::DICTIONARY);
        if (0 == http_code_)
        {
			result.SetKey("error", base::Value(WR_ERROR_HTTP_CODE_EMPTY));
        }
        else
        {
            result.SetKey("error", base::Value(http_code_));
        }

        std::move(callback_).Run(std::move(signature_), std::move(result), this);
        return;
    }

	if (WalletHttpCallType::URL == signature_->get_type())
	{
		base::Value result(base::Value::Type::DICTIONARY);

		const std::string &image_data = data_.str();

		if (0 == image_data.size())
		{
			result.SetKey("error", base::Value(WR_ERROR_HTTP_CODE_EMPTY));
		}
		else
		{
			std::string image_data_base64;
			base::Base64Encode(std::string({image_data.begin(), image_data.end()}), &image_data_base64);

			result.SetStringPath("data_base64", image_data_base64);
            result.SetStringPath("url", signature_->get_method_name());
		}

		std::move(callback_).Run(std::move(signature_), std::move(result), this);
		return;
	}

	absl::optional<base::Value> json = base::JSONReader::Read(data_.str());
	data_.clear();

	if (json == absl::nullopt)
	{
		base::Value result(base::Value::Type::DICTIONARY);
		result.SetKey("error", base::Value(WR_ERROR_JSON_NOT_VALID));

        std::string errortext = std::to_string(http_code_) + " " + base::MD5String(signature_->get_method_name() + "method");
		result.SetKey("errortext", base::Value(errortext));

		std::move(callback_).Run(std::move(signature_), std::move(result), this);
		return;
	}

	base::Value result = signature_->post_process(json->Clone(), http_code_);

	std::move(callback_).Run(std::move(signature_), std::move(result), this);
}

void WalletRequest::OnRetry(base::OnceClosure start_retry)
{
    NOTREACHED();
}

}