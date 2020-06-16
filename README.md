# Socks5BalancerAsio
A Simple TCP Socket Balancer for balance Multi Socks5 Proxy Backend Powered by Boost.Asio

---

**This is a C++ implement (porting) of [Socks5Balancer](https://github.com/Lyoko-Jeremie/Socks5Balancer) , Powered by Boost.Asio .**

Some code about how to use Boost.Asio & openssl come from [trojan](https://github.com/trojan-gfw/trojan) & [ArashPartow's C++ TCP Proxy server](https://github.com/ArashPartow/proxy) , Thank you~  
The code style come from [philave's BOOST_SOCKS5 Proxy Server](https://github.com/philave/boost_socks5) , Thank you~  

---
## Monitor

![Monitor Screen](https://github.com/Lyoko-Jeremie/Socks5BalancerAsio/wiki/monitor-screen.png)

---

## porting reason

when i run the origin `Socks5Balancer` on a qemu of archlinux vm with 2G disk / 500MB memory / 2 Core continue for 24x7 ,  
i find that it may crash on some times by memory limit and will hang hole system when connect create rate > 70/s .  
so i write this, hope it can work more faster and more stable.  

