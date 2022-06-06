document.addEventListener("DOMContentLoaded", function() {

    let iteration = 0;
    let delay = 500;
    let dot = 0;

    function getTimeout() {
        if (0 == iteration % 2) {
            return 0;
        }

        let timeout = Math.floor(10 * 1000 * Math.random());
    
        log('timeout for ' + timeout + ' ms');

        return timeout;
    }

    function log(msg, includeconsole=true) {
        if (includeconsole) {
            console.log('iteration ' + iteration + ' ' + msg);
        }
        document.getElementById('status').innerText = 'iteration ' + iteration + ' ' + msg;
    }

    function check_error(e) {
        if ('undefined' !== typeof e.detail.error && null !== e.detail.error) {
            show_error(data);
            return true;
        }
        return false;
    }

    function show_error(data) {
        let msg = 'FAILED: ';

        if (data.error) {
            msg = msg + ' ' + data.error;
        }
        if (data.errortext) {
            msg = msg + ' ' + data.errortext;
        }

        log(msg);
    }

    function environment(e, event_name, callback) {
        if (check_error(e)) {
            return;
        }

        if (0 === dot) {
            log('checking environment for ' + event_name, false);
            dot = 1;
        }
        else {
            log('checking environment for ' + event_name + '..', false);
            dot = 0;
        }

        if (true === e.detail.is_loaded) {
            return callback();
        }

        setTimeout(() => {
            chrome.send('environment', [event_name, {}])
          }, delay);
    }

    function next() {
        iteration = iteration + 1;
        if (iteration > parseInt(document.getElementById('iteration').value))
        {
            return log('*** success ***');
        }

        chrome.send('environment', ['environment', {}]);
    }

    /* do not remove  
    chrome.send = (name, params) => {
        setTimeout(() => {
            if ('environment' === name) {
                window.dispatchEvent(new CustomEvent(params[0], {detail: {is_loaded: true}}));
            }
            else if ('web_url' === name) {
                window.dispatchEvent(new CustomEvent(params[0], {detail: {is_loaded: true}}));
            }
            else {
                switch(params[0]) {
                    case 'mnsync': 
                        return window.dispatchEvent(new CustomEvent(params[1], {detail: {IsBlockchainSynced: true}}));
                    case 'getbalance': 
                        return window.dispatchEvent(new CustomEvent(params[1], {detail: {result: 20}}));
                    case 'transactions':
                        return window.dispatchEvent(new CustomEvent(params[1], {detail: {transactions: []}}));
                    case 'listdapps':
                        return window.dispatchEvent(new CustomEvent(params[1], {detail: {result: []}}));
                }

                window.dispatchEvent(new CustomEvent(params[1], {detail: {}}));
            }
        }, 0);
    }
    */

    document.getElementById('start').onclick = () => {
        document.getElementById('start').style.display = 'none';
        chrome.send('environment', ['environment', {}]);
    };

    window.addEventListener('environment', (e) => {
        if (true !== e.detail.is_loaded){
            return show_error({'error': 'wallet is not ready'});
        }

        if (0 === iteration % 2) {
            log('sethdseed1');
            chrome.send('universal', ['sethdseed', 'sethdseed', [document.getElementById('mnemonic1').value]])
        }
        else {
            log('sethdseed2');
            chrome.send('universal', ['sethdseed', 'sethdseed', [document.getElementById('mnemonic2').value]])
        }
        
    });

    window.addEventListener('sethdseed', () => {
        chrome.send('environment', ['sethdseed_environment', {}])
    });

    window.addEventListener('sethdseed_environment', (e) => {
        environment(e, 'sethdseed_environment', () => { chrome.send('universal', ['mnsync', 'mnsync', ["status"]]) });
    });

    window.addEventListener('mnsync', (e) => {
        if ("undefined" === typeof e.detail.IsBlockchainSynced) {
            return show_error({'error': 'mnsync failed'});
        }

        setTimeout(() => {
            chrome.send('universal', ['encryptwallet', 'encryptwallet', ['12345678']]);
        }, getTimeout());
    });

    window.addEventListener('encryptwallet', (e) => {
        log('checking encryptwallet');
        chrome.send('environment', ['encryptwallet_environment', {}])
    });

    window.addEventListener('encryptwallet_environment', (e) => {
        environment(e, 'encryptwallet_environment', () => {     
            log('checking getnewaddress');
            chrome.send('universal', ['getnewaddress', 'getnewaddress', []]); 
        });
    });

    window.addEventListener('getnewaddress', (e) => {
        if (check_error(e)){
            return;
        }

        log('checking stop');
        setTimeout(() => {
            chrome.send('universal', ['stop', 'stop', []]); 
        }, getTimeout());
    });

    window.addEventListener('stop', (e) => {
        chrome.send('environment', ['stop_environment', {}])
    });

    window.addEventListener('stop_environment', (e) => {
        environment(e, 'stop_environment', () => { chrome.send('universal', ['getbalance', 'getbalance', []]); });
    });

    window.addEventListener('getbalance', (e) => {
        if (check_error(e)){
            return;
        }

        if ('number' !== typeof e.detail.result) {
            return show_error({'balance': 'failed to get balance'});
        }

        log('balance is ' + e.detail.result);

        log('checking restartwallet');
        setTimeout(() => {
            chrome.send('service', ['restartwallet', 'restartwallet', {}]);
        }, getTimeout());
    });

    window.addEventListener('restartwallet', (e) => {
        chrome.send('environment', ['restartwallet_environment', {}])
    });

    window.addEventListener('restartwallet_environment', (e) => {
        environment(e, 'restartwallet_environment', () => { 
            log('checking getbalance2'); 
            chrome.send('universal', ['getbalance', 'getbalance2', []]); 
        });
    });

    window.addEventListener('getbalance2', (e) => {
        if (check_error(e)){
            return;
        }

        if ('number' !== typeof e.detail.result) {
            return show_error({'balance': 'failed to get balance'});
        }

        log('balance2 is ' + e.detail.result);

        log('checking transactions');
        setTimeout(() => {
            chrome.send('service', ['transactions', 'transactions', {'limit': 20}]);
            chrome.send('service', ['transactions', 'skip1', {'limit': 20, addresses: {}}]);
            chrome.send('service', ['transactions', 'skip2', {'limit': 20, addresses: {address_to:{}}}]);
            chrome.send('service', ['transactions', 'skip3', {'limit': 20, addresses: {address_to:[]}}]);
            chrome.send('service', ['transactions', 'skip4', {'limit': 20, 'category': null}]);
            chrome.send('service', ['transactions', 'skip5', {'limit': 20, 'text': null}]);
        }, getTimeout());
    });

    window.addEventListener('transactions', (e) => {
        if (check_error(e)){
            return;
        }

        if ('undefined' === typeof e.detail.transactions ||
            !Array.isArray(e.detail.transactions))
        {
            return show_error({'error': 'failed to get transactions'});
        }

        chrome.send('web_simple', ['wallet_images', 'wallet_images', 'GET']);
    });

    window.addEventListener('wallet_images', (e) => {
        log('checking web_url');
        chrome.send('web_url', ['web_url', 'https://netbox.global/favicon.ico', 'GET']);
    });

    window.addEventListener('web_url', (e) => {
        log('checking listdapps');
        chrome.send('universal', ['listdapps', 'listdappsfirst', [1]]);
        chrome.send('universal', ['listdapps', 'sendtonull', [1]]);
    });

    window.addEventListener('listdappsfirst', (e) => {
        if (check_error(e)) {
            return;
        }        

        if ('undefined' === typeof e.detail.result || !Array.isArray( e.detail.result))
        {
            return show_error('failed to check first dapplist');
        }

        log('dapplist first length is ' + e.detail.result.length);

        chrome.send('universal', ['listdapps', 'listdappssecond', [1]]);

        next();
    });
});