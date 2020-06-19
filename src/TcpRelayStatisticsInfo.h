/**
 * Socks5BalancerAsio : A Simple TCP Socket Balancer for balance Multi Socks5 Proxy Backend Powered by Boost.Asio
 * Copyright (C) <2020>  <Jeremie>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef SOCKS5BALANCERASIO_TCPRELAYSTATISTICSINFO_H
#define SOCKS5BALANCERASIO_TCPRELAYSTATISTICSINFO_H

#ifdef MSVC
#pragma once
#endif

#include <boost/asio.hpp>
#include <memory>
#include <string>
#include <utility>
#include <list>
#include <map>
#include <atomic>


class TcpRelaySession;

class TcpRelayStatisticsInfo : public std::enable_shared_from_this<TcpRelayStatisticsInfo> {
public:
    struct Info : public std::enable_shared_from_this<Info> {
        std::list<std::weak_ptr<TcpRelaySession>> sessions;

        std::atomic_size_t byteUp = 0;
        std::atomic_size_t byteDown = 0;
        size_t byteUpLast = 0;
        size_t byteDownLast = 0;
        size_t byteUpChange = 0;
        size_t byteDownChange = 0;
        size_t byteUpChangeMax = 0;
        size_t byteDownChangeMax = 0;

        std::atomic_size_t connectCount{0};

        void removeExpiredSession();

        void closeAllSession();

        void calcByte();

        void connectCountAdd();

        void connectCountSub();

        size_t calcSessionsNumber();
    };

private:
    std::map<size_t, std::shared_ptr<Info>> upstreamIndex;
    std::map<std::string, std::shared_ptr<Info>> clientIndex;
    std::map<std::string, std::shared_ptr<Info>> listenIndex;

public:
    std::atomic_size_t lastConnectServerIndex{0};

public:
    std::map<size_t, std::shared_ptr<Info>> &getUpstreamIndex();

    std::map<std::string, std::shared_ptr<Info>> &getClientIndex();

    std::map<std::string, std::shared_ptr<Info>> &getListenIndex();

public:
    void addSession(size_t index, std::weak_ptr<TcpRelaySession> s);

    void addSessionClient(std::string addr, std::weak_ptr<TcpRelaySession> s);

    void addSessionListen(std::string addr, std::weak_ptr<TcpRelaySession> s);

    std::shared_ptr<Info> getInfo(size_t index);

    std::shared_ptr<Info> getInfoClient(std::string addr);

    std::shared_ptr<Info> getInfoListen(std::string addr);

    void removeExpiredSession(size_t index);

    void removeExpiredSessionClient(std::string addr);

    void removeExpiredSessionListen(std::string addr);

    void addByteUp(size_t index, size_t b);

    void addByteUpClient(std::string addr, size_t b);

    void addByteUpListen(std::string addr, size_t b);

    void addByteDown(size_t index, size_t b);

    void addByteDownClient(std::string addr, size_t b);

    void addByteDownListen(std::string addr, size_t b);

    void connectCountAdd(size_t index);

    void connectCountAddClient(std::string addr);

    void connectCountAddListen(std::string addr);

    void connectCountSub(size_t index);

    void connectCountSubClient(std::string addr);

    void connectCountSubListen(std::string addr);

    void calcByteAll();

    void removeExpiredSessionAll();

    void closeAllSession(size_t index);

    void closeAllSessionClient(std::string addr);

    void closeAllSessionListen(std::string addr);

};


#endif //SOCKS5BALANCERASIO_TCPRELAYSTATISTICSINFO_H
