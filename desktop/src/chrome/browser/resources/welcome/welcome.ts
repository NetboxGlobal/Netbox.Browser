(function(){ 
    const button = document.getElementById('start');
    if (button) {
        button.onclick = () => 
        {
            // @ts-ignore
            chrome.runtime.netboxOpenWallet();
        }
    }
})();
        
