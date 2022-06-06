import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

Polymer({
    is: 'netboxinfo-app',

    _template: html`<div>
    <input type="button" id="copybutton" value="Copy Info to Clipboard">
</div>
<div class="info-block">
    <ul id="data" style="overflow-wrap: break-word"></ul>
    <ul id="operating_system_type" style="overflow-wrap: break-word"></ul>
    <ul id="file_paths" style="overflow-wrap: break-word"></ul>
    <ul id="wallet_paths" style="overflow-wrap: break-word"></ul>
    <ul id="disks_free_space" style="overflow-wrap: break-word"></ul>
</div>`,

    attached() {
		this.$.copybutton.onclick = function()
		{
			// Make sure nothing is selected
			window.getSelection().removeAllRanges();

			document.execCommand('selectAll');
			document.execCommand('copy');

			// And deselect everything at the end.
			window.getSelection().removeAllRanges();

		};

		window.addEventListener('netboxinfo.common',     	this.on_netbox_info.bind(this));
		window.addEventListener('netboxinfo.filepaths',  	this.on_file_paths.bind(this));
		window.addEventListener('netboxinfo.diskfreespace', this.on_disks_free_space.bind(this));
		window.addEventListener('netboxinfo.ostype', 		this.on_operating_system_type.bind(this));
		window.addEventListener('netboxinfo.walletpaths', 	this.on_wallet_paths_check.bind(this));

		chrome.send('netboxinfo', []);
    },


	on_netbox_info: function(e)
	{
		let data = e['detail'] || [];
		
		let parent_element = this.$.data;

		for(let key in data)
		{
			let el = document.createElement('li');
			el.innerHTML =  "<b>" + key + ":</b> " + data[key];
			parent_element.insertBefore(el, parent_element.firstChild);

			if (key === "Browser log" || key === "Wallet log")
			{
				let key2 = key;

				let btn = document.createElement("button");
				btn.classList.add("button");
				btn.onclick = () => chrome.send("openfile", [key2]);
				btn.innerHTML = "Open";

				el.appendChild(btn);
			}
		}
	},

	on_file_paths: function(e)
	{
		let data = e['detail'] || [];
		
		let parent_element = this.$.file_paths;

		for(let key in data)
		{
			let el = document.createElement('li');
			el.innerHTML = "<b>" + key + ":</b> " + data[key];
			parent_element.insertBefore(el, parent_element.firstChild);
		}
	},

	on_disks_free_space: function(e)
	{
		let data = e['detail'] || [];
		
		let parent_element = this.$.disks_free_space;

		for(let key in data)
		{
			let el = document.createElement('li');
			el.innerHTML = "<b>" + data[key]["Path"] + ":</b> " + data[key]["Volume"];
			parent_element.insertBefore(el, parent_element.firstChild);
		}
	},

	on_operating_system_type: function(e)
	{
		let data = e['detail'] || [];
		
		let parent_element = this.$.operating_system_type;

		for(let key in data)
		{
			let el = document.createElement('li');
			el.innerHTML = "<b>" + key + ":</b> " + data[key];
			parent_element.insertBefore(el, parent_element.firstChild);
		}
	},

	on_wallet_paths_check: function(e)
	{
		let data = e['detail'] || [];
		
		let parent_element = this.$.wallet_paths;

		for(let key in data)
		{
			let el = document.createElement('li');
			el.innerHTML = "<b>" + key + ":</b> " + data[key];
			parent_element.insertBefore(el, parent_element.firstChild);
		}
	}

});

