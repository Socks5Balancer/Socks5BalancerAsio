# Socks5BalancerAsio
A Simple TCP Socket Balancer for balance Multi Socks5 Proxy Backend Servers Powered by Boost.Asio

---

Docker version available with instructions here: [fossforreal/Socks5BalancerAsio-Docker](https://github.com/fossforreal/Socks5BalancerAsio-Docker)

---

**This is a C++ implement (porting) of [Socks5Balancer](https://github.com/Lyoko-Jeremie/Socks5Balancer) , Powered by Boost.Asio .**

Some code about how to use Boost.Asio & openssl come from [trojan](https://github.com/trojan-gfw/trojan) & [ArashPartow's C++ TCP Proxy server](https://github.com/ArashPartow/proxy) , Thank you~  
The code style come from [philave's BOOST_SOCKS5 Proxy Server](https://github.com/philave/boost_socks5) , Thank you~  

---
## Monitor

![Monitor Screen](https://github.com/Lyoko-Jeremie/Socks5BalancerAsio/wiki/monitor-screen.png)

## Features
1. support http-proxy/socks5-proxy on same port (aka mixed-port mode), now you never need run another tools to covert socks5 proxy to http proxy
1. support listen on multi port
1. Load Balance user connect to multi backend with multi rule
1. Load all config from config file
1. support unlimited backend number
1. can monitor backend alive state
1. can monitor backend is work well
1. connection from front end will not connect to dead backend
1. have 2 check way to check a backend is dead or alive
1. if a backend revive from dead, it will be auto enable
1. can config rule per client-ip / per listener
1. with random delay on every test. so all the test will not burst out at a same time. this feature avoid make large load on remote server.

## the Rule of Load Balance now support
1. `loop` \: every connect will round robin on all live backend
1. `random` \: every connect will random connect a live backend
1. `one_by_one` \: every new connect will try to use last connect backend, if that backend dead, will try next live one
1. `change_by_time` \: similar `one_by_one` , but will force change server to next after a specific time (in config)

## config
config is a json, the template config is `config.json`

it must encode with `UTF-8 no BOM`

```json5
{
  "listenHost": "127.0.0.1",                // the main listen host, default is 127.0.0.1
  "listenPort": 5000,                       // the main listen port, default is 5000
  // (if all backend down at same time, the new connect will wast `retryTimes*connectTimeout` time on it. )
  "retryTimes": 3,                          // when new connect coming, how many backend we try to connect. default is 3
  "connectTimeout": 2000,                   // when try to connect backend, how long the backend connect timeout means it's dead. default is 2000ms (2s)
  // (notice that, the testRemoteHost must be a HTTPS server. now we only check is it connectable.)
  "testRemoteHost": "www.google.com",       // what target host name we use to check the backend socks5 proxy work well. default is "www.google.com"
  "testRemotePort": 443,                    // what target port we use to check the backend socks5 proxy work well. default is 443
  // (the `tcpCheck` only create a TCP handshake to backend to check the backend is alive)
  "tcpCheckPeriod": 5000,                   // how many Period of every backend live check. default is 5000ms (5s)
  "tcpCheckStart": 1000,                    // how many time we wait before first backend live check. default is 1000ms (1s)
  "connectCheckPeriod": 300000,             // how many Period of check every backend socks5 proxy work well. default is 300000ms (300s or 5min)
  "connectCheckStart": 1000,                // how many time we wait before first check backend socks5 proxy work well. default is 1000ms (1s)
  "additionCheckPeriod": 10000,             // how many time we check the all backend dead and force check then. default is 10000ms (10s)
  "upstreamSelectRule": "random",           // the Load Balance Rule. default is `random`
  "sleepTime": 1800000,                     // how many time we need to sleep after last connect. default is 1800000ms (1800s or 30min)
  "threadNum": 0,                           // multi-thread num, >3 means use multi thread mode. default is 0 (means not use multi-thread).
                                            //    NOTES: the `thread_real_use = system_thread_num - 1` , if system_thread_num <2 or this config <2 , will use single thread.
                                            //    NOTES: it use boost_thread group, if build with `NOT_USE_BOOST_THEAD` , multi-thread will always disable
  "serverChangeTime": 5000,                 // the config of Load Balance Rule `change_by_time`. default is 5000ms (5s)
  "stateServerHost": "127.0.0.1",           // the simple state monitor server host. default is 127.0.0.1
  "stateServerPort": 5010,                  //  the simple state monitor server port. default is 5010
  // if follow 3 config all are `true`, will run in plain mode, like a pure tcp relay
  "disableConnectTest": false,              // disable the two connect test, default is false
  "disableConnectionTracker": false,        // disable connect analysis, default is false
  "traditionTcpRelay": false,               // disable mixed-port mode, default is false
  "multiListen": [                          // the addition listen ports, will work same as main listen port, default is empty
    {
      "host": "127.0.0.1",
      "port": 6600
    },
    {
      "host": "127.0.0.1",
      "port": 6601
    },
    {
      "host": "127.0.0.1",
      "port": 6602
    }
  ],
  "upstream": [                             // the backend server array.  default is empty.
    //  (now only support socks5 proxy server with full function, but it can be any type tcp backend if use traditionTcpRelay=true disableConnectTest=true disableConnectionTracker=true)
    {
      "name": "Server Name A",              // the backend server name string, you can use any string on this
      "host": "127.0.0.1",                  // the backend server host
      "port": 3000                          // the backend server port
    },
    {
      "name": "Server Name B",
      "host": "127.0.0.1",
      "port": 3001,
      "slowImpl": 1                         // compatible bad/slow server impl (like Golang), it maybe not send all data in one package
    },
    {
      "name": "Server Name C",
      "host": "127.0.0.1",
      "port": 3002,
      "disable": 1                          // if set the "disable" as NOT Falsy (Falsy means 0,false,null,undefined..),  this server will disable by default
    }
  ],
  "EmbedWebServerConfig": {                 // the embedding web server, it work as a static file provider
    "enable": false,                        // is enable . default is false
    "host": "127.0.0.1",                    // the embedding web server listen host. default is 127.0.0.1
    "port": 5002,                           // the embedding web server listen port. default is 5002
    "root_path": "./html/",                 // the embedding web server file provider root path. default is "./html/"
                                            //     all file access will be limit on this directory.
                                            //     Warning!!! make sure dont have any sensitive file or symlink on this dir.
                                            //     Notice: now the server will not decode url, so it will cannot find file that file name contained non-ascii characters or other need be encode strings
    "index_file_of_root": "state.html",     // the `index` file of "/" path, it special how to provide the root page. default is "state.html".
                                            //     Carefully!!!, it must is a relative path as `root_path`.
    "backendHost": "",                      // special the stateServerHost, it use to construct the backend json string. default is empty string, will auto fill by stateServerHost
    "backendPort": 0,                       // special the stateServerPort, it use to construct the backend json string. default is 0, will auto fill by stateServerPort
    "allowFileExtList": "htm html js json jpg jpeg png bmp gif ico svg"
                                            // the allow file ext list can be access, split by ' ' (empty space)
  }
}
```

the minimal config template :

```json
{
  "listenHost": "127.0.0.1",
  "listenPort": 5000,
  "upstreamSelectRule": "random",
  "serverChangeTime": 5000,
  "upstream": [
    {
      "host": "127.0.0.1",
      "port": 3000
    }
  ]
}

```

you can find other minimal config template on the `example-config` dir

**Note:** if you want use this to balance any tcp protocol backend that not socks5 , 
you can set ```
  "disableConnectTest": true,
  "disableConnectionTracker": true,
  "traditionTcpRelay": true ``` ,
then it will run as a pure tcp relay or pure tcp balance or multi port bundle.
but in that case, the `upstreamSelectRule` must be `random` or `loop`,
any other rule will not work as expected because of the backend measure are disabled.


## how to run it ?

you can set the command params to special config file path

follow is systemd config example



set config to command params
```
# nano /etc/systemd/system/Socks5BalancerAsio.service

[Unit]
Description=Socks5BalancerAsio Service
Requires=network.target
After=network.target

[Service]
Type=simple
User=username
Restart=always
AmbientCapabilities=CAP_NET_BIND_SERVICE
WorkingDirectory=/home/username/Socks5BalancerAsio/
ExecStart=/home/username/Socks5BalancerAsio/Socks5BalancerAsio /home/username/Socks5BalancerAsio/config.json

[Install]
WantedBy=multi-user.target
```


---

## Notice

if a backend cannot connect to the `testRemoteHost`, this backend cannot pass the `connectCheck`.

if a backend cannot be handshake use TCP, this backend cannot pass the `tcpCheck`.

if a backend  cannot pass `connectCheck` or `tcpCheck`, this backend will be disable untill it pass all the two check.

if all backend are disabled, the front connect will be reject.

**Warning: the `connectCheck` have a long CheckPeriod (default is 5min), if a backend cannot work well in the Period, connect to it will han and the Balance unable to discover it.**

**Warning: Many socks5 Proxy have a strange behavior, the first connect will hang or wait many time after a long idle.**

**Warning: if you set the `connectCheckPeriod` too short, it may wast many bandwidth.**

---

## which rule is best ?

 if you all server have good performance , use `change_by_time` .
 
 if you have some best server than other , use `one_by_one` .

if you want the IP not change, example playing some game, use `one_by_one`

if you use it to download something and dont care about latency and IP, like BT, use `random` or `loop`


---

## monitor it running state

a simple state monitor Json Api server can config by `stateServerHost` and `stateServerPort`

if you want use the Monitor html, only need open the html in any browser and fill the backend input with the `stateServer` on your config. 
then, it will get server info json from main server , and control it with a `/op?xxx` path on same place. 

if you think the simple Monitor html looks so ugly, you can re-write it with any other tools, 
only need to follow the data process way on the simple Monitor html. 

BTW: the simple Monitor html in the `./html` folder write with html5 and css with javascript and use `Vue.js` , addition libs `lodash.js` and `moment.js` only give it some help function to process data or format date. 

_(`Vue.js` is a good replace for jQ at small project , i only use it's data and event binding on this place. it let me not to setup Angular on there .)_


## Use Monitor Page On EmbedWebServer

if you dont want to install a addition web server , and you use the web page on safe env, you can use the EmbedWebServer.
 
the EmbedWebServer only do two action:    

1. provide `backend json` on path `/backend`. 
2. be a static file provider like nginx

you can config it in the config file of `EmbedWebServerConfig` segment.


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
## Struct Info
now the main server write with pure c++ (c++17) , powered by Boost.Asio/Boost.Beast . 
and Monitor Powered by Boost.Beast, it run with RestApi serve by main server and a alone html web page ( you can find it on `./html` folder) . 

if you want use the Monitor html, only need open the html in any browser and fill the backend input with the `stateServer` on your config. 
then, it will get server info json from main server , and control it with a `/op?xxx` path on same place. 

if you think the simple Monitor html looks so ugly, you can re-write it with any other tools, 
only need to follow the data process way on the simple Monitor html. 

BTW: the simple Monitor html in the `./html` folder write with html5 and css with javascript and use `Vue.js` , addition libs `lodash.js` and `moment.js` only give it some help function to process data or format date. 

_(`Vue.js` is a good replace for jQ at small project , i only use it's data and event binding on this place. it let me not to setup Angular on there .)_

---

## how to build & dev

### Dependencies

**CMake** >= 3.16  
**Boost** >= 1.74
> For older versions Boost v1.70+ (v1.73 is recommended as last working version)
> Checkout at tag "v1.0" or commit "e6c491ce56f6de21423c5d780d1c8865714fabe3"

**OpenSSL** >= 1.1.0 recommend 1.1.1h  
**MSVC** or **GCC** , required C++17 support  

#### Build on Windows

install VS2019

if you dont want build Boost and OpenSSL by yourself , download Prebuild version from follow :

Boost Prebuild : 
- https://sourceforge.net/projects/boost/files/boost-binaries/

OpenSSL Prebuild : 
- https://wiki.openssl.org/index.php/Binaries
- https://kb.firedaemon.com/support/solutions/articles/4000121705


```
cmake -DBOOST_INCLUDEDIR=<path_to>/boost_1_73_0 -DOPENSSL_ROOT_DIR=<path_to>\openssl-1.1.1h\x64
```

then build it

#### Build on ArchLinux

```
pacman -S base-devel --needed
pacman -S openssl
pacman -S cmake
```

```bash
# IF YOU WANT TO BUILD WITH LATEST BOOST (v1.74+)
pacman -S boost
```

> **If you are building older version, run:**
>
> ```bash
> # DOWNLOAD & BUILD BOOST v1.73 FROM SOURCE
> wget -nc https://boostorg.jfrog.io/artifactory/main/release/1.73.0/source/boost_1_73_0.tar.bz2
> tar --skip-old-files -jxf boost_1_73_0.tar.bz2
> cd boost_1_73_0
> ./bootstrap.sh
> ./b2 link=static
> PATH=$(pwd):$(pwd)/stage/lib:$PATH
> cd ..
> ```



```
cd ./Socks5BalancerAsio
cmake .
make
```

then all ok

#### Build on Debian

```bash
# FOR LATEST BOOST (v1.74+):
apt update && apt upgrade
apt install -y libboost-all-dev git cmake libssl-dev g++
git clone https://github.com/fossforreal/Socks5BalancerAsio
cd Socks5BalancerAsio
cmake .
make -j$(nproc)

```

or

```bash
# FOR BOOST v1.71-1.73:
apt update && apt upgrade
apt install -y git cmake libssl-dev g++ wget bzip2

# DOWNLOAD & BUILD BOOST v1.73 FROM SOURCE
wget -nc https://boostorg.jfrog.io/artifactory/main/release/1.73.0/source/boost_1_73_0.tar.bz2
tar --skip-old-files -jxf boost_1_73_0.tar.bz2
cd boost_1_73_0
./bootstrap.sh
./b2 link=static
PATH=$(pwd):$(pwd)/stage/lib:$PATH
cd ..

# CHECK THIS REPO AT TAG v1.0
git clone https://github.com/fossforreal/Socks5BalancerAsio
cd Socks5BalancerAsio
git checkout e6c491ce56f6de21423c5d780d1c8865714fabe3
cmake .
make -j$(nproc)
```


### Dev

Recommend use **Clion**

---

## porting reason

when i run the origin [Socks5Balancer](https://github.com/Lyoko-Jeremie/Socks5Balancer) on a qemu of archlinux vm with 2G disk / 500MB memory / 2 Core continue for 24x7 ,  
i find that it may crash on some times by memory limit and will hang hole system when connect create rate > 70/s .  
so i write this, hope it can work more faster and more stable.  

## TODO
- [x] listen on multi port
- [x] http proxy To socks5 proxy
- [x] http/socks5 on same port (mixed-port mode)
- [ ] analysis socks5 protocol and communicate protocol type
- [ ] monitor per connect info
- [ ] backend latency analysis 
- [x] port aggregation mode(multi-listen only mode)
- [x] direct provide Monitor Page (embedding nginx)

- [x] ~~support socks5 UDP~~ (socks5 UDP will work well if use on localhost or on LAN)
- [ ] support Load Balance Rule `best_latency`
- [ ] support Load Balance Rule `fast_download_speed`
- [ ] support Load Balance Rule `fast_upload_speed`
- [ ] add server priority
- [x] add control function on state monitor
- [x] add manual disable function on state monitor
- [X] add runtime rule change function on state monitor
- [x] add manual close connect function on state monitor
- [ ] auto close all connect when manual switch (change) server
- [X] show connect speed on state monitor
- [X] show how many data was transfer in data monitor
- [ ] Analysis socks5 proxy
- [ ] Direct Impl Proxy Client
- [x] ~~Impl Trojan Proxy Client~~ KISS(keep it simple stupid)

## The Background story
for a long time ago, i want to find a simple tools to balance on many proxy. it need auto chose the best proxy and dont let dead one to slow the internet speed .  
but when i search all over the internet , i cannot find a simple , alone , light , fast , stable , easy to use and free tools to complete this requirement.  
so, i write it by myself, i named it as `socks5 balancer`.   

this tools build in top of Boost.Asio and C++, it's a single executable file, can work in both Windows and Linux, and can load balance for socks5 backend or pure tcp backend, provide a socks5/http/https multi in one port support, and it can detect the backend socks5 proxy health state similar like the HAProxy. also, it can work on multi-to-one mode, this mode run like a invert-load balance. and it can easy monitor and control through the http interface.  

this project still in develop, and i am using it every day every night to test the stability . now it continue run in my Home's network for a mouth ago without any error or crush.   
i will add more feature on it and continue make it more stable. and i hope it can help some one like me.  

---
the background story:  
I am a develop in a special country, for well-known reasons, i need use proxy to access the international worldwide internet to contribution code for humanity.  
but because of the poor,  i don't have many money to by a expensive stable proxy server, the only one way is that collect many low price or free proxy and try they one by one hope some of it can work.  
so, i write this project as a Open Source Software, hope it can help other one who face the same dilemma as me.  

---
follow is a prototype i wrote with Typescript in top of NodeJs :  
https://github.com/Lyoko-Jeremie/Socks5Balancer  
Because the performance of Node and JavaScript. it not stable, not fast, not lightly, so i rewrite it with C++/Boost.Asio.  

---
BTW, you can see the `.idea` dir in my every project. so , you can know how i love jetbrains so far.

---
The Clion / WebStorm / IDEA / RSharp Tools is the world-class best IDE(Integrated Development Environment) in my heart .  
Thank To The Jetbrains send me a All Pack License to let me continue to use the Best Tools In The World.   
[
<img alt="Jetbrains Logo" src="https://github.com/Lyoko-Jeremie/Socks5BalancerAsio/wiki/jetbrains-variant-4.png" width="200">
](https://www.jetbrains.com/?from=Socks5Balancer)  

