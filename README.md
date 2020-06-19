# Socks5BalancerAsio
A Simple TCP Socket Balancer for balance Multi Socks5 Proxy Backend Powered by Boost.Asio

---

**This is a C++ implement (porting) of [Socks5Balancer](https://github.com/Lyoko-Jeremie/Socks5Balancer) , Powered by Boost.Asio .**

Some code about how to use Boost.Asio & openssl come from [trojan](https://github.com/trojan-gfw/trojan) & [ArashPartow's C++ TCP Proxy server](https://github.com/ArashPartow/proxy) , Thank you~  
The code style come from [philave's BOOST_SOCKS5 Proxy Server](https://github.com/philave/boost_socks5) , Thank you~  

---
## Monitor

![Monitor Screen](https://github.com/Lyoko-Jeremie/Socks5BalancerAsio/wiki/monitor-screen.png)

## Struct Info
now the main server write with pure c++ (c++11/17) , powered by Boost.Asio/Boost.Beast .  
and Monitor Powered by Boost.Beast, it run with RestApi serve by main server and a alone html web page ( you can find it on `./html` folder) .  

if you want use the Monitor html, only need open the html in any browser and fill the backend input with the `stateServer` on your config.  
then, it will get server info json from main server , and control it with a `/op?xxx` path on same place. 

if you think the simple Monitor html looks so ugly, you can re-write it with any other tools,  
only need to follow the data process way on the simple Monitor html.  
BTW: the simple Monitor html in the `./html` folder write with html5 and css with javascript and use `Vue.js` , addition libs `lodash.js` and `moment.js` only give it some help function to process data or format date.  
<small>(`Vue.js` is a good replace for jQ at small project , i only use it's data and event binding on this place. it let me not to setup Angular on there .)</small>

---

## Use Monitor Page On Nginx

if you want,  
you can setup main server on a server then serve the monitor html page on a nginx server like me.  

you can config nginx to serve a json string on path `./backend` like follow ,
 and then the monitor page will load it to config the backend (if the backend not special on url search params).

NOTE: the `host` is not require, it can lost or be a empty string like follow, but the `port` must special.

```
        location ~ ^/backend {
            default_type application/json;
            return 200 '{"host":"","port":6660}';
        }
        location / {
            root   /usr/share/nginx/S5BA;
            index  index.html index.html state.html;
        }
```

when page start,  
the monitor page will get the backend config from the the url search params,   
otherwise, try to get the backend config from page server path `backend` .

---

## porting reason

when i run the origin `Socks5Balancer` on a qemu of archlinux vm with 2G disk / 500MB memory / 2 Core continue for 24x7 ,  
i find that it may crash on some times by memory limit and will hang hole system when connect create rate > 70/s .  
so i write this, hope it can work more faster and more stable.  

## TODO
- [x] listen on multi port
- [x] http proxy To socks5 proxy
- [x] http/socks5 on same port (mixed-port mode)
- [ ] analysis socks5 protocol and communicate protocol type
- [ ] monitor connect info
- [ ] backend latency analysis 
