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

#include "TcpRelayStatisticsInfo.h"
#include "TcpRelayServer.h"

void TcpRelayStatisticsInfo::Info::removeExpiredSession() {
    auto it = sessions.begin();
    while (it != sessions.end()) {
        if ((*it).expired()) {
            it = sessions.erase(it);
        } else {
            ++it;
        }
    }
}

void TcpRelayStatisticsInfo::Info::closeAllSession() {
    auto it = sessions.begin();
    while (it != sessions.end()) {
        auto p = (*it).lock();
        if (p) {
            p->forceClose();
            ++it;
        } else {
            it = sessions.erase(it);
        }
    }
}

void TcpRelayStatisticsInfo::Info::calcByte() {
    size_t newByteUp = byteUp.load();
    size_t newByteDown = byteDown.load();
    byteUpChange = newByteUp - byteUpLast;
    byteDownChange = newByteDown - byteDownLast;
    byteUpLast = newByteUp;
    byteDownLast = newByteDown;
    if (byteUpChange > byteUpChangeMax) {
        byteUpChangeMax = byteUpChange;
    }
    if (byteDownChange > byteDownChangeMax) {
        byteDownChangeMax = byteDownChange;
    }
}

void TcpRelayStatisticsInfo::Info::connectCountAdd() {
    ++connectCount;
}

void TcpRelayStatisticsInfo::Info::connectCountSub() {
    --connectCount;
}

size_t TcpRelayStatisticsInfo::Info::calcSessionsNumber() {
    removeExpiredSession();
    return sessions.size();
}

void TcpRelayStatisticsInfo::addSession(size_t index, std::weak_ptr<TcpRelaySession> s) {
    if (upstreamIndex.find(index) == upstreamIndex.end()) {
        upstreamIndex.try_emplace(index, std::make_shared<Info>());
    }
    upstreamIndex.at(index)->sessions.push_back(s);
}

void TcpRelayStatisticsInfo::addSessionClient(std::string addr, std::weak_ptr<TcpRelaySession> s) {
    if (clientIndex.find(addr) == clientIndex.end()) {
        clientIndex.try_emplace(addr, std::make_shared<Info>());
    }
    clientIndex.at(addr)->sessions.push_back(s);
}

void TcpRelayStatisticsInfo::addSessionListen(std::string addr, std::weak_ptr<TcpRelaySession> s) {
    if (listenIndex.find(addr) == listenIndex.end()) {
        listenIndex.try_emplace(addr, std::make_shared<Info>());
    }
    listenIndex.at(addr)->sessions.push_back(s);
}

std::shared_ptr<TcpRelayStatisticsInfo::Info> TcpRelayStatisticsInfo::getInfo(size_t index) {
    auto n = upstreamIndex.find(index);
    if (n != upstreamIndex.end()) {
        return n->second;
    } else {
        return {};
    }
}

std::shared_ptr<TcpRelayStatisticsInfo::Info> TcpRelayStatisticsInfo::getInfoClient(std::string addr) {
    auto n = clientIndex.find(addr);
    if (n != clientIndex.end()) {
        return n->second;
    } else {
        return {};
    }
}

std::shared_ptr<TcpRelayStatisticsInfo::Info> TcpRelayStatisticsInfo::getInfoListen(std::string addr) {
    auto n = listenIndex.find(addr);
    if (n != listenIndex.end()) {
        return n->second;
    } else {
        return {};
    }
}

void TcpRelayStatisticsInfo::removeExpiredSession(size_t index) {
    auto p = getInfo(index);
    if (p) {
        p->removeExpiredSession();
    }
}

void TcpRelayStatisticsInfo::removeExpiredSessionClient(std::string addr) {
    auto p = getInfoClient(addr);
    if (p) {
        p->removeExpiredSession();
    }
}

void TcpRelayStatisticsInfo::removeExpiredSessionListen(std::string addr) {
    auto p = getInfoListen(addr);
    if (p) {
        p->removeExpiredSession();
    }
}

void TcpRelayStatisticsInfo::addByteUp(size_t index, size_t b) {
    auto p = getInfo(index);
    if (p) {
        p->byteUp += b;
    }
}

void TcpRelayStatisticsInfo::addByteUpClient(std::string addr, size_t b) {
    auto p = getInfoClient(addr);
    if (p) {
        p->byteUp += b;
    }
}

void TcpRelayStatisticsInfo::addByteUpListen(std::string addr, size_t b) {
    auto p = getInfoListen(addr);
    if (p) {
        p->byteUp += b;
    }
}

void TcpRelayStatisticsInfo::addByteDown(size_t index, size_t b) {
    auto p = getInfo(index);
    if (p) {
        p->byteDown += b;
    }
}

void TcpRelayStatisticsInfo::addByteDownClient(std::string addr, size_t b) {
    auto p = getInfoClient(addr);
    if (p) {
        p->byteDown += b;
    }
}

void TcpRelayStatisticsInfo::addByteDownListen(std::string addr, size_t b) {
    auto p = getInfoListen(addr);
    if (p) {
        p->byteDown += b;
    }
}

void TcpRelayStatisticsInfo::calcByteAll() {
    for (auto &a: upstreamIndex) {
        a.second->calcByte();
    }
    for (auto &a: clientIndex) {
        a.second->calcByte();
    }
    for (auto &a: listenIndex) {
        a.second->calcByte();
    }
}

void TcpRelayStatisticsInfo::removeExpiredSessionAll() {
    for (auto &a: upstreamIndex) {
        a.second->removeExpiredSession();
    }
    for (auto &a: clientIndex) {
        a.second->removeExpiredSession();
    }
    for (auto &a: listenIndex) {
        a.second->removeExpiredSession();
    }
}

void TcpRelayStatisticsInfo::closeAllSession(size_t index) {
    auto p = getInfo(index);
    if (p) {
        p->closeAllSession();
    }
}

void TcpRelayStatisticsInfo::closeAllSessionClient(std::string addr) {
    auto p = getInfoClient(addr);
    if (p) {
        p->closeAllSession();
    }
}

void TcpRelayStatisticsInfo::closeAllSessionListen(std::string addr) {
    auto p = getInfoListen(addr);
    if (p) {
        p->closeAllSession();
    }
}

void TcpRelayStatisticsInfo::connectCountAdd(size_t index) {
    auto p = getInfo(index);
    if (p) {
        p->connectCountAdd();
    }
}

void TcpRelayStatisticsInfo::connectCountAddClient(std::string addr) {
    auto p = getInfoClient(addr);
    if (p) {
        p->connectCountAdd();
    }
}

void TcpRelayStatisticsInfo::connectCountAddListen(std::string addr) {
    auto p = getInfoListen(addr);
    if (p) {
        p->connectCountAdd();
    }
}

void TcpRelayStatisticsInfo::connectCountSub(size_t index) {
    auto p = getInfo(index);
    if (p) {
        p->connectCountSub();
    }
}

void TcpRelayStatisticsInfo::connectCountSubClient(std::string addr) {
    auto p = getInfoClient(addr);
    if (p) {
        p->connectCountSub();
    }
}

void TcpRelayStatisticsInfo::connectCountSubListen(std::string addr) {
    auto p = getInfoListen(addr);
    if (p) {
        p->connectCountSub();
    }
}

std::map<size_t, std::shared_ptr<TcpRelayStatisticsInfo::Info>> &TcpRelayStatisticsInfo::getUpstreamIndex() {
    return upstreamIndex;
}

std::map<std::string, std::shared_ptr<TcpRelayStatisticsInfo::Info>> &TcpRelayStatisticsInfo::getClientIndex() {
    return clientIndex;
}

std::map<std::string, std::shared_ptr<TcpRelayStatisticsInfo::Info>> &TcpRelayStatisticsInfo::getListenIndex() {
    return listenIndex;
}
