function netbox_send_event(event_name, event_data) {
    window.dispatchEvent(new CustomEvent(event_name, {detail: event_data}));
}

let news = {

    last_slide_:0,
    slide_interval_:7000,

    max_tab_items_: 4,
    lang_: "en",

    init: function()
    {
        window.addEventListener('news',     this.on_get_news.bind(this));
        window.addEventListener('image',    this.on_get_image.bind(this));
        window.addEventListener('setlang',  this.on_change_lang.bind(this));
        window.addEventListener('getlang',  this.on_get_lang.bind(this));

        this.last_slide_ = Date.now();

        this.init_texts_switch();
        this.init_lang();

        // init slider
        setTimeout(() => this.on_image_slider(true), this.slide_interval_);

        chrome.send('universal', ['getlang']);
    },

    on_get_lang(e)
    {
        let data = e['detail'] || [];

        if (true === data['success'])
        {
            this.lang_ = data['lang'];
        }

        chrome.send('universal', ['news', this.lang_]);
    },

    on_image_slider: function(e)
    {
        if (true === e)
        {
            if (Date.now() - this.last_slide_ < this.slide_interval_)
            {
                setTimeout(() => this.on_image_slider(true), Date.now() - this.last_slide_);
                return;
            }
            else
            {
                setTimeout(() => this.on_image_slider(true), this.slide_interval_);
            }
        }

        this.last_slide_ = Date.now();

        let active_el = document.querySelector('.slider__item.active');

        if (!active_el)
        {
            return;
        }

        let next_el = active_el.nextSibling;


        while(next_el && next_el.nodeType != 1)
        {
            next_el = next_el.nextSibling;
        }

        if (!next_el)
        {
            next_el = document.querySelector('.slider__item');
        }

        if (!next_el)
        {
            return;
        }

        active_el.classList.remove('active');
        next_el.classList.add('active');
    },

    init_texts_switch: function()
    {
        let elem = document.querySelector('.texts__buttons');
        if (elem)
        {
            elem.onclick = function(event)
            {
                let elem = document.querySelector('.texts');
                if (!elem || !event || !event.target)
                {
                    return;
                }

                if (elem.classList.contains('picked') && event.target.classList.contains('latest'))
                {

                    elem.classList.remove('picked');
                    elem.classList.add('latest');

                    return;
                }

                if (elem.classList.contains('latest') && event.target.classList.contains('picked'))
                {
                    elem.classList.remove('latest');
                    elem.classList.add('picked');

                    return;
                }

            }
        }

    },

    on_get_news: function(e)
    {
        let data = e['detail'] || [];

        if (true !== data['success'])
        {
            return;
        }

        data = data['data'] || [];
        if ('undefined' === typeof data['editors_pick'])
        {
            return;
        }

        let editors_data = data['editors_pick'];

        let slider = document.getElementById('slider');
        slider.innerHTML = '';
        let slider_template = document.querySelector('.templates .slider__item');

        let picked_tab = document.querySelector('.texts__tab.picked ul');
        picked_tab.innerHTML = '';
        let tab_item_template = document.querySelector('.templates .texts__tab-item');

        let current_tab_items_count = 0;

        for(i in editors_data)
        {
            // append
            let image_item = slider_template.cloneNode(true);

            let image_id = 'slider__item-img' + current_tab_items_count;

            image_item.querySelector(".slider__item-a").innerHTML = editors_data[i].title;
            image_item.querySelector(".slider__item-a").setAttribute('href', editors_data[i].link);
            image_item.querySelector(".slider__item-img").setAttribute('id', image_id);
            image_item.querySelector(".slider__item-author").innerHTML = editors_data[i].creator;

            image_item.onclick = (e) => this.on_image_slider(e);

            slider.appendChild(image_item);

            // picked tab
            if (current_tab_items_count < this.max_tab_items_)
            {
                let picked_item = tab_item_template.cloneNode(true);
                picked_item.querySelector('a').setAttribute('href', editors_data[i].link);
                picked_item.querySelector('a').innerHTML = editors_data[i].title;

                picked_tab.appendChild(picked_item);
            }

            chrome.send('universal', ['image', editors_data[i].imageUrl, image_id]);

            current_tab_items_count++;
        }

        let first_slider_item = slider.querySelector('.slider__item');
        if (first_slider_item)
        {
            first_slider_item.classList.add('active');
        }


        // latest part

        let latest_tab = document.querySelector('.texts__tab.latest ul');
        latest_tab.innerHTML = '';

        current_tab_items_count = 0;

        let latest_data = data['latest_news'];

        for (i in latest_data)
        {
            if (current_tab_items_count < this.max_tab_items_)
            {
                let latest_item = tab_item_template.cloneNode(true);
                latest_item.querySelector('a').setAttribute('href', latest_data[i].link);
                latest_item.querySelector('a').innerHTML = latest_data[i].title;

                latest_tab.appendChild(latest_item);

            }

            current_tab_items_count++;
        }

        this.last_slide_ = Date.now();
    },

    on_get_image: function(e)
    {
        let data = e['detail'] || [];

        if (true !== data['success'])
        {
            return;
        }

        let img = document.getElementById(data['selector']);

        if (img)
        {
            img.setAttribute('src', data['image']);

            if (!document.querySelector('body').classList.contains('loaded'))
            {
                document.querySelector('body').classList.add('loaded');
            }
        }
    },

    change_lang: function(event)
    {
        if (!event || !event.target)
        {
            return;
        }

        let lang_title = document.getElementById('lang__button-desc');
        if (lang_title)
        {
            lang_title.innerHTML = event.target.innerHTML;

            this.lang_ = event.target.getAttribute('langid');

            chrome.send('universal', ['setlang', this.lang_]);
        }

        document.querySelector('.lang').classList.remove('open');

        let elem = document.querySelector('.lang__list .active');
        if (elem)
        {
            elem.classList.remove('active');
        }

        event.target.classList.add('active');

    },

    on_change_lang: function(e)
    {
        chrome.send('universal', ['news', this.lang_]);

        if ('ar' === this.lang_)
        {
            if (!document.body.classList.contains('rtl'))
            {
                document.body.classList.add('rtl');
            }
        }
        else
        {
            if (document.body.classList.contains('rtl'))
            {
                document.body.classList.remove('rtl');
            }
        }
    },

    show_lang_menu: function(event)
    {
        if(!event.target.closest('#lang__button'))
        {
            if (document.querySelector('.lang').classList.contains('open'))
            {
                document.querySelector('.lang').classList.remove('open');
            }
        }
        else
        {
            document.querySelector('.lang').classList.toggle('open');
        }
    },

    on_message: function(event)
    {
        if (document.querySelector('.lang').classList.contains('open'))
        {
            document.querySelector('.lang').classList.remove('open');
        }
    },

    init_lang: function()
    {
        let el_list = document.querySelectorAll('.lang__list li');
        el_list.forEach((element) => { element.onclick = (e) => this.change_lang(e); });

        window.onclick = (event) => { this.show_lang_menu(event); }

        window.addEventListener("message", this.on_message.bind(this), false);
    }


};

window.onload = function()
{
    news.init();
};
