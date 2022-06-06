function wallet_send_event(event_name, event_data)
{
    window.dispatchEvent(new CustomEvent(event_name, {detail: event_data}));
}
