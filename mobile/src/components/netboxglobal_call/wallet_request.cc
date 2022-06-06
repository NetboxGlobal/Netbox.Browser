#include "components/netboxglobal_call/wallet_request.h"

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/netboxglobal_call/netbox_error_codes.h"
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
        content::BrowserContext::GetDefaultStoragePartition(ProfileManager::GetLastUsedProfile())->GetURLLoaderFactoryForBrowserProcess();

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
    if (!success && http_code_ != 404 && http_code_ != 422)
    {
        base::Value result(base::Value::Type::DICTIONARY);
        if (0 == http_code_)
        {
            result.SetKey("error", base::Value(WR_ERROR_HTTP_CODE_EMPTY));
            if (signature_.get())
            {
                LOG(WARNING) << "600, Empty response, method: " << signature_->get_method_name();
            }
        }
        else
        {
            result.SetKey("error", base::Value(http_code_));
        }

        std::move(callback_).Run(std::move(signature_), std::move(result), this);
        return;
    }

    base::Optional<base::Value> json = base::JSONReader::Read(data_.str());
    data_.clear();

    if (json == base::nullopt)
    {
        base::Value result(base::Value::Type::DICTIONARY);
        result.SetKey("error", base::Value(WR_ERROR_JSON_NOT_VALID));
        result.SetKey("error_text", base::Value(std::to_string(http_code_)));
        std::move(callback_).Run(std::move(signature_), std::move(result), this);
        return;
    }

    base::Value result_json = signature_->post_process(json->Clone(), http_code_);

    std::move(callback_).Run(std::move(signature_), std::move(result_json), this);
}

void WalletRequest::OnRetry(base::OnceClosure start_retry)
{
    NOTREACHED();
}

}