#include "chrome/browser/netbox/wallet_manager/wallet_manager.h"

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/base64.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/ui/webui/wallet/wallet_dom_handler.h"
#include "chrome/browser/transaction_service/transaction_service.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/browser_task_traits.h"
#include "components/netboxglobal_verify/netboxglobal_verify.h"
#include "components/netboxglobal_utils/utils.h"
#include "components/version_info/version_info.h"
#include "net/base/load_flags.h"
#include "net/base/escape.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_context.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

static const int32_t REQUEST_BALANCE_INTERVAL_SEC = 5;

static const int32_t REQUEST_FIRST_ADDRESS_DEFAULT_INTERVAL_SEC = 5;

static const int32_t WALLET_PROCESS_RESTART_DELAY = 5;

namespace Netboxglobal
{

WalletManager::WalletManager() : request_first_address_web_interval_sec_(REQUEST_FIRST_ADDRESS_DEFAULT_INTERVAL_SEC)
{
    universal_methods_white_list =
    {
        "add_dapp",
        "add_dapp_image",
        "buy",
        "buy_status",
        "delete_dapp",
        "edit_dapp",
        "get_dapp",
        "get_dapp_config",
        "get_dapp_thumbnails",
        "get_dapps_info",
        "get_email_by_first_address",
        "get_system_addresses",
        "get_system_addresses2",
        "get_system_features",
        "lottery_address",
        "lottery_last_pending_block",
        "stake",
        "staking_status",
        "unstake",
        "vote_dapp_against",
        "vote_dapp_against_revert",
        "vote_dapp_for",
        "vote_dapp_for_revert",
        "wallet_images"
    };
}

WalletManager::~WalletManager()
{
}

bool WalletManager::start()
{
    g_browser_process->env_controller()->add_data_observer(std::bind(&WalletManager::on_environment_ready, this, std::placeholders::_1, std::placeholders::_2));

    return true;
}

bool WalletManager::stop()
{
    polling_timer_.Stop();

    return true;
}

void WalletManager::add_handler(IWalletTabHandler* handler)
{
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    {
        std::lock_guard<std::recursive_mutex> grd(calls_mutex_);

        handlers_.insert(handler);
    }
}

void WalletManager::remove_handler(IWalletTabHandler* handler)
{
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    {
        std::lock_guard<std::recursive_mutex> grd(calls_mutex_);

        handlers_.erase(handler);
    }
}

void WalletManager::on_environment_ready(WalletSessionManager::DataState data_state, const WalletSessionManager::Data &data)
{
	DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

	if (data_state == WalletSessionManager::DataState::DS_NONE)
	{
		is_loaded = false; // NETBOXTODO remove
		return;
	}

	if (data_state == WalletSessionManager::DataState::DS_OK)
	{
		environment_error = 0;

		base::PostTask(
			FROM_HERE,
			{content::BrowserThread::UI, base::MayBlock(), base::TaskPriority::BEST_EFFORT, base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
			base::BindOnce(&WalletManager::initialize_rpc_first_address, base::Unretained(this))
		);

		return;
	}

	if (WalletSessionManager::AUTH_NOT_LOGGED == g_browser_process->env_controller()->get_auth_state())
	{
		notify_auth();
		notify_status_changed();
		return;
	}

	environment_error = (int)data_state;

	notify_status_changed();

	if (data_state == WalletSessionManager::DataState::DS_WALLET_FILE_NOT_FOUND)
	{
		base::PostDelayedTask(
			FROM_HERE,
			{
				content::BrowserThread::UI,
				base::MayBlock(),
				base::TaskPriority::BEST_EFFORT,
				base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN
			},
			base::BindOnce(&WalletManager::schedule_wallet_stop_and_start, base::Unretained(this)),
			base::TimeDelta::FromSeconds(WALLET_PROCESS_RESTART_DELAY)
		);
	}
}

void WalletManager::schedule_wallet_stop_and_start()
{
    is_loaded = false;

    notify_status_changed();

	base::PostTask(
		FROM_HERE,
		{
			content::BrowserThread::UI,
			base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN
		},
		base::BindOnce(&WalletSessionManager::wallet_start, base::Unretained(g_browser_process->env_controller()), WALLET_LAUNCH::STOP_AND_START)
	);
}

void WalletManager::schedule_wallet_wait_and_start()
{
	is_loaded = false;

    notify_status_changed();

	base::PostTask(
		FROM_HERE,
		{
			content::BrowserThread::UI,
			base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN
		},
		base::BindOnce(&WalletSessionManager::wallet_start, base::Unretained(g_browser_process->env_controller()), WALLET_LAUNCH::WAIT_AND_START)
	);
}

void WalletManager::add_first_address_observer(FirstAddressEventObserversList::value_type val)
{
    first_address_observers_.push_back(val);
}

void WalletManager::notify_first_addres_observers()
{
	DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    for (const auto &observer : first_address_observers_)
        observer(first_address_);
}

void WalletManager::notify_status_changed()
{
	DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

	int32_t status = 0;

	if (WalletSessionManager::AUTH_NOT_LOGGED == g_browser_process->env_controller()->get_auth_state())
	{
		status = WalletSessionManager::DataState::DS_AUTH_COOKIE_ERROR;
	}
    else if (!is_loaded)
    {
        status = WalletSessionManager::DataState::DS_NONE;
    }
	else if (environment_error)
	{
		status = environment_error;
	}

    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_NETBOX_WALLET_STATUS_CHANGED,
        content::NotificationService::AllSources(),
        content::Details<int32_t>(&status));
}

void WalletManager::notify_balance_changed(double new_balance)
{
	DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_NETBOX_UPDATE_BALANCE,
        content::NotificationService::AllSources(),
        content::Details<double>(&new_balance));
}


// pinging functions
void WalletManager::initialize_rpc_first_address()
{
	DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    ping_count_++;

	std::unique_ptr<Netboxglobal::WalletHttpCallSignature> signature(new Netboxglobal::WalletHttpCallSignature(Netboxglobal::WalletHttpCallType::RPC_JSON));
	signature->set_method_name("getfirstaddress");
	signature->set_params(base::Value(base::Value::Type::LIST));

	request_http(std::move(signature));
}

void WalletManager::notify_auth()
{
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    base::Value result(base::Value::Type::DICTIONARY);
    result.SetKey("is_auth", base::Value(false));
    if (g_browser_process->env_controller()->is_qa())
    {
        result.SetKey("is_qa", base::Value("1"));
    }
    process_results_to_js_broadcast("environment", std::move(result));
}

void WalletManager::process_results_to_js_broadcast(std::string event_name, base::Value results)
{
	DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    for(auto handler_iterator = handlers_.begin(); handler_iterator != handlers_.end(); ++handler_iterator)
    {
        base::Value data(base::Value::Type::DICTIONARY);
        (*handler_iterator)->OnTabCall(event_name, results.Clone());
    }
}



// general request functions
void WalletManager::environment(IWalletTabHandler* handler, const std::string &event_name)
{
	DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

	base::DictionaryValue data;

	// QA
    if (g_browser_process->env_controller()->is_qa())
    {
        data.SetKey("is_qa", base::Value("1"));
    }

	if (WalletSessionManager::AUTH_NOT_LOGGED == g_browser_process->env_controller()->get_auth_state())
	{
		data.SetKey("is_auth", base::Value(false));
	}

	if (0 != environment_error){
		data.SetKey("error", base::Value(environment_error));
		handler->OnTabCall(event_name, std::move(data));
		return;
	}

	// Loaded
	if (!is_loaded){
		data.SetKey("is_loaded", base::Value(false));
		handler->OnTabCall(event_name, std::move(data));
		return;
	}

	data.SetKey("is_loaded", base::Value(true));

	if (WalletSessionManager::AUTH_LOGGED == g_browser_process->env_controller()->get_auth_state())
	{
		data.SetKey("is_auth", base::Value(true));
	}
	else
	{
		data.SetKey("is_auth", base::Value(false));
	}

    if (g_browser_process->env_controller()->is_first_launch())
    {
        data.SetKey("is_new", base::Value("1"));
    }

    if (g_browser_process->env_controller()->is_debug())
    {
        data.SetKey("debug", base::Value("1"));
    }

	data.SetKey("first_address", base::Value(first_address_));

    handler->OnTabCall(event_name, std::move(data));
}

void WalletManager::request_balance()
{
	DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

	std::unique_ptr<Netboxglobal::WalletHttpCallSignature> signature(new Netboxglobal::WalletHttpCallSignature(Netboxglobal::WalletHttpCallType::RPC_JSON));
	signature->set_method_name("getbalance");
	signature->set_params(base::Value(base::Value::Type::LIST));

	request_http(std::move(signature));
}

void WalletManager::service(IWalletTabHandler* handler, std::string method_name, std::string event_name, base::Value params)
{
	DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    if ("transactions" == method_name)
    {
        std::unique_ptr<WalletHttpCallSignature> request(new WalletHttpCallSignature(WalletHttpCallType::RPC_JSON));
        request->set_event_name(event_name);
        request->set_ui_handler(handler);
        request->set_params(std::move(params));

        return g_browser_process->transaction_service()->ui_get_transactions(std::move(request));
    }

	if ("preshow" == method_name)
	{
		Netboxglobal::preshowwallet();

		if (handler)
		{
			handler->OnTabCall("preshow", base::Value(base::Value::Type::DICTIONARY));
		}
		return;
	}

	if ("restartwallet" == method_name)
	{
		schedule_wallet_stop_and_start();

		if (handler)
		{
			handler->OnTabCall("restartwallet", base::Value(base::Value::Type::DICTIONARY));
		}

		return;
	}

	VLOG(NETBOX_LOG_LEVEL) << "Unhandled method";
}

void WalletManager::request_http(std::unique_ptr<WalletHttpCallSignature> &&signature)
{
	DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    std::unique_ptr<WalletRequest> http_request = std::make_unique<WalletRequest>();
    WalletRequest* http_request_ptr = http_request.get();

    if (g_browser_process->env_controller()->is_qa())
    {
        signature->set_qa(true);
    }

	if (WalletHttpCallType::RPC_JSON == signature->get_type() || WalletHttpCallType::RPC_RAW == signature->get_type())
	{
		if (!is_loaded || 0 != environment_error)
		{
			if ("getfirstaddress" != signature->get_method_name()
				&& "environment" != signature->get_method_name() )
			{
				base::Value result(base::Value::Type::DICTIONARY);

				if (environment_error)
				{
					result.SetIntKey("error", environment_error);
				}
				else
				{
					result.SetIntKey("error", WalletSessionManager::DataState::DS_NONE);
				}

				IWalletTabHandler* handler  = signature->get_ui_handler();

				if (handler)
				{
					std::string event_name      = signature->get_event_name();
					end_http_call(handler, std::move(result), event_name);
				}

				return;
			}
		}

		std::string access_token_base64;
		g_browser_process->env_controller()->get_wallet_rpc_credentails(access_token_base64);

		signature->set_rpc_token(access_token_base64);
	}

	if (WalletHttpCallType::API_SIGNED == signature->get_type())
	{
		if ("create_wallet" == signature->get_method_name())
		{
			signature->append_param("guid", base::Value(g_browser_process->env_controller()->get_guid()));
		}

		if ("get_system_addresses" == signature->get_method_name() || "get_system_features" == signature->get_method_name())
		{
			signature->append_param("browser_version", base::Value(version_info::GetVersionNumber()));
		}

		signature->append_param("first_address", base::Value(first_address_));
	}

    http_request->start(std::move(signature),
        base::BindOnce(&WalletManager::on_http_response, base::Unretained(this)));

    ui_requests_[http_request_ptr] = std::move(http_request);
}

void WalletManager::on_http_response(std::unique_ptr<WalletHttpCallSignature> signature, base::Value results, WalletRequest* http_request_ptr)
{
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

	if (http_request_ptr)
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
	}

    if (WalletHttpCallType::API_SIGNED == signature->get_type() && results.is_dict())
    {

        absl::optional<int> error = results.FindIntKey("error");
        if (absl::nullopt != error && 401 == *error)
        {
            notify_auth();
        }
    }

	if (WalletHttpCallType::RPC_JSON == signature->get_type())
    {
		if ("stop" 				        == signature->get_method_name()
         || "encryptwallet"             == signature->get_method_name())
        {
            schedule_wallet_wait_and_start();
        }
		else {                      
            if ("sethdseed"                 == signature->get_method_name()) 
            {
                // 
                absl::optional<int> error = results.FindIntKey("error");
                if (absl::nullopt == error || -4 != *error) 
                {
                    schedule_wallet_wait_and_start();
                }
            }
			if ("getfirstaddress" != signature->get_method_name() && results.FindKey("netboxrestart"))
			{
				schedule_wallet_stop_and_start();
			}
			else if ("getbalance" == signature->get_method_name() && results.FindKey("result"))
			{
				end_get_balance_rpc(&results);
			}
		}
    }

    IWalletTabHandler* handler  = signature->get_ui_handler();

    if (!handler)
    {
		if (WalletHttpCallType::RPC_JSON == signature->get_type())
		{
			if ("getfirstaddress" == signature->get_method_name())
			{
				end_ping_first_address_rpc(&results); // TODO, share
			}
		}

        return;
    }

	std::string event_name      = signature->get_event_name();
	end_http_call(handler, std::move(results), event_name); // TODO, rename
}

void WalletManager::end_http_call(IWalletTabHandler* handler, base::Value data, std::string event_name) // TODO rename
{
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    std::lock_guard<std::recursive_mutex> grd(calls_mutex_);

    auto it = handlers_.find(handler);

    if (handlers_.end() != it)
    {
        handler->OnTabCall(event_name, std::move(data));
    }
}

void WalletManager::end_get_balance_rpc(const base::Value* json_data)
{
	DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

	if (!json_data || !json_data->is_dict())
	{
		return;
	}

	absl::optional<double> result = json_data->FindDoublePath("result");
    if (absl::nullopt == result)
    {
		return;
	}

	double result_value = *result;

	base::PostTask(
		FROM_HERE,
		{
			content::BrowserThread::UI,
			base::MayBlock(),
			base::TaskPriority::BEST_EFFORT,
			base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN
		},
		base::BindOnce(&WalletManager::notify_balance_changed, base::Unretained(this), result_value)
	);
}

void WalletManager::end_ping_first_address_rpc(const base::Value* json_data)
{
	DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

	if (!json_data || !json_data->is_dict())
	{
		return;
	}

	const std::string* first_address_raw = json_data->FindStringKey("result");

	if (first_address_raw)
    {
		is_loaded = true;
		if (first_address_ != *first_address_raw)
		{
			first_address_ = *first_address_raw;
			notify_first_addres_observers();
		}

		notify_status_changed();

        ping_count_ = 0;

		if (!polling_timer_.IsRunning())
		{
			polling_timer_.Start(FROM_HERE,
                        base::TimeDelta::FromSeconds(REQUEST_BALANCE_INTERVAL_SEC),
                        this,
                        &WalletManager::request_balance);
		}

        return;
    }

    base::PostDelayedTask(
        FROM_HERE,
        {
            content::BrowserThread::UI,
            base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN
        },
        base::BindOnce(&WalletSessionManager::wallet_start, base::Unretained(g_browser_process->env_controller()), WALLET_LAUNCH::START),
        base::TimeDelta::FromSeconds(REQUEST_FIRST_ADDRESS_DEFAULT_INTERVAL_SEC));
}

}