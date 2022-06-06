#ifndef COMPONENTS_NETBOXGLOBAL_CALL_WALLET_REQUEST_H_
#define COMPONENTS_NETBOXGLOBAL_CALL_WALLET_REQUEST_H_

#include <map>

#include "base/bind.h"
#include "chrome/browser/netbox/call/wallet_http_call_signature.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/cpp/simple_url_loader_stream_consumer.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace Netboxglobal
{

class WalletRequest : public network::SimpleURLLoaderStreamConsumer
{
public:
    WalletRequest();
    ~WalletRequest() override;
    void start(std::unique_ptr<WalletHttpCallSignature>, base::OnceCallback<void(std::unique_ptr<WalletHttpCallSignature> signature, base::Value result, WalletRequest*)> callback);

    // SimpleURLLoaderStreamConsumer interface
    void OnDataReceived(base::StringPiece string_piece, base::OnceClosure resume) override;
    void OnComplete(bool success) override;
    void OnRetry(base::OnceClosure start_retry) override;

protected:
    int32_t http_code_ = 0;
    void clear();

    base::OnceCallback<void(std::unique_ptr<WalletHttpCallSignature> signature, base::Value result, WalletRequest*)> callback_;
    std::unique_ptr<WalletHttpCallSignature> signature_;
    std::stringstream data_;
    std::unique_ptr<network::SimpleURLLoader> sender_;

    void on_http_response_started(const GURL& final_url, const network::mojom::URLResponseHead& response_head);
};

}

#endif