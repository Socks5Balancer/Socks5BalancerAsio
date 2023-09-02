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
#include "TcpRelaySession.h"
#include <chrono>
#include <limits>

void TcpRelayStatisticsInfo::Info::removeExpiredSession() {
    std::lock_guard lg{sessionsMtx};
    auto it = sessions.begin();
    while (it != sessions.end()) {
        if ((*it).ptr.expired()) {
            it = sessions.erase(it);
        } else {
            ++it;
        }
    }
}

void TcpRelayStatisticsInfo::Info::closeAllSession() {
    std::lock_guard lg{sessionsMtx};
    auto it = sessions.begin();
    while (it != sessions.end()) {
        auto p = (*it).ptr.lock();
        if (p) {
            p->forceClose();
            ++it;
        } else {
            it = sessions.erase(it);
        }
    }
}

void TcpRelayStatisticsInfo::Info::calcByte() {
    std::lock_guard lg{calcByteMtx};
    size_t newByteUp = byteUp.load();
    size_t newByteDown = byteDown.load();
    byteUpChange = newByteUp - byteUpLast;
    byteDownChange = newByteDown - byteDownLast;
    byteUpLast = newByteUp;
    byteDownLast = newByteDown;
    if (byteUpChange > byteUpChangeMax) {
        byteUpChangeMax = byteUpChange.load();
    }
    if (byteDownChange > byteDownChangeMax) {
        byteDownChangeMax = byteDownChange.load();
    }
}

void TcpRelayStatisticsInfo::Info::connectCountAdd() {
    ++connectCount;
}

void TcpRelayStatisticsInfo::Info::connectCountSub() {
    --connectCount;
}

size_t TcpRelayStatisticsInfo::Info::calcSessionsNumber() {
    std::lock_guard lg{sessionsMtx};
    BOOST_LOG_S5B(trace) << "TcpRelayStatisticsInfo::Info::calcSessionsNumber() Before:" << sessions.size();
    removeExpiredSession();
    BOOST_LOG_S5B(trace) << "TcpRelayStatisticsInfo::Info::calcSessionsNumber() After:" << sessions.size();
    return sessions.size();
}

TcpRelayStatisticsInfo::SessionInfo::SessionInfo(const std::shared_ptr<TcpRelaySession> &s) :
        upstreamIndex(s ? s->getNowServer()->index : std::numeric_limits<decltype(upstreamIndex)>::max()),
        clientEndpointAddrPortString(s ? s->getClientEndpointAddrPortString() : ""),
        listenEndpointAddrString(s ? s->getListenEndpointAddrString() : ""),
        rawPtr(s ? s.get() : nullptr),
        ptr(s ? s : nullptr),
        startTime(duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()
        ) {
    BOOST_ASSERT(s);
    updateTargetInfo(s);
}

TcpRelayStatisticsInfo::SessionInfo::SessionInfo(const std::weak_ptr<TcpRelaySession> &s)
        : SessionInfo(s.lock()) {}

void TcpRelayStatisticsInfo::SessionInfo::updateTargetInfo(const std::shared_ptr<TcpRelaySession> &s) {
    if (const auto &p = s) {
        auto pp = p->getTargetEndpointAddr();
        host = pp.first;
        post = pp.second;
        targetEndpointAddrString = p->getTargetEndpointAddrString();
        if (s->authUser) {
            authUserId = s->authUser->id;
        }
    }
}

void TcpRelayStatisticsInfo::addSession(size_t index, const std::shared_ptr<TcpRelaySession> &s) {
    BOOST_ASSERT(s);
    std::lock_guard lgG{mtx};
    if (auto ptr = s) {
        if (upstreamIndex.find(index) == upstreamIndex.end()) {
            upstreamIndex.try_emplace(index, std::make_shared<Info>());
        }
        auto n = upstreamIndex.at(index);
        std::lock_guard lg{n->sessionsMtx};
        BOOST_LOG_S5B_ID(s->relayId, trace)
            << "TcpRelayStatisticsInfo::addSession()"
            << " getClientEndpointAddrString:"
            << ptr->getClientEndpointAddrString()
            << " getClientEndpointAddrPortString:"
            << ptr->getClientEndpointAddrPortString()
            << " getListenEndpointAddrString:"
            << ptr->getListenEndpointAddrString();
        BOOST_ASSERT(n->sessions.get<SessionInfo::ListenClientAddrPortPair>().find(std::make_tuple(
                ptr->getClientEndpointAddrPortString(),
                ptr->getListenEndpointAddrString()
        )) == n->sessions.get<SessionInfo::ListenClientAddrPortPair>().end());
        n->sessions.emplace_back(s);
        if (auto ns = ptr->getNowServer()) {
            std::lock_guard lgLM{n->lastUseUpstreamIndexMtx};
            n->lastUseUpstreamIndex = ns->index;
        }
    }
}

void TcpRelayStatisticsInfo::addSessionClient(const std::shared_ptr<TcpRelaySession> &s) {
    BOOST_ASSERT(s);
    std::lock_guard lgG{mtx};
    if (auto ptr = s) {
        const std::string &addr = ptr->getClientEndpointAddrString();
        if (clientIndex.find(addr) == clientIndex.end()) {
            clientIndex.try_emplace(addr, std::make_shared<Info>());
        }
        auto n = clientIndex.at(addr);
        std::lock_guard lg{n->sessionsMtx};
        BOOST_LOG_S5B_ID(s->relayId, trace)
            << "TcpRelayStatisticsInfo::addSessionClient()"
            << " getClientEndpointAddrString:"
            << ptr->getClientEndpointAddrString()
            << " getClientEndpointAddrPortString:"
            << ptr->getClientEndpointAddrPortString()
            << " getListenEndpointAddrString:"
            << ptr->getListenEndpointAddrString();
        BOOST_ASSERT(n->sessions.get<SessionInfo::ListenClientAddrPortPair>().find(std::make_tuple(
                ptr->getClientEndpointAddrPortString(),
                ptr->getListenEndpointAddrString()
        )) == n->sessions.get<SessionInfo::ListenClientAddrPortPair>().end());
        n->sessions.emplace_back(s);
        if (auto ns = ptr->getNowServer()) {
            std::lock_guard lgLM{n->lastUseUpstreamIndexMtx};
            n->lastUseUpstreamIndex = ns->index;
        }
    }
}

void TcpRelayStatisticsInfo::addSessionListen(const std::shared_ptr<TcpRelaySession> &s) {
    BOOST_ASSERT(s);
    std::lock_guard lgG{mtx};
    if (auto ptr = s) {
        const std::string &addr = ptr->getListenEndpointAddrString();
        if (listenIndex.find(addr) == listenIndex.end()) {
            listenIndex.try_emplace(addr, std::make_shared<Info>());
        }
        auto n = listenIndex.at(addr);
        std::lock_guard lg{n->sessionsMtx};
        BOOST_LOG_S5B_ID(s->relayId, trace)
            << "TcpRelayStatisticsInfo::addSessionListen()"
            << " getClientEndpointAddrString:"
            << ptr->getClientEndpointAddrString()
            << " getClientEndpointAddrPortString:"
            << ptr->getClientEndpointAddrPortString()
            << " getListenEndpointAddrString:"
            << ptr->getListenEndpointAddrString();
        BOOST_ASSERT(n->sessions.get<SessionInfo::ListenClientAddrPortPair>().find(std::make_tuple(
                ptr->getClientEndpointAddrPortString(),
                ptr->getListenEndpointAddrString()
        )) == n->sessions.get<SessionInfo::ListenClientAddrPortPair>().end());
        n->sessions.emplace_back(s);
        if (auto ns = ptr->getNowServer()) {
            std::lock_guard lgLM{n->lastUseUpstreamIndexMtx};
            n->lastUseUpstreamIndex = ns->index;
        }
    }
}

void TcpRelayStatisticsInfo::addSessionAuthUser(const std::shared_ptr<TcpRelaySession> &s) {
    BOOST_ASSERT(s);
    std::lock_guard lgG{mtx};
    if (auto ptr = s) {
        BOOST_ASSERT(ptr->authUser);
        size_t id = ptr->authUser->id;
        if (authUserIndex.find(id) == authUserIndex.end()) {
            authUserIndex.try_emplace(id, std::make_shared<Info>());
        }
        auto n = authUserIndex.at(id);
        std::lock_guard lg{n->sessionsMtx};
        BOOST_LOG_S5B_ID(s->relayId, trace)
            << "TcpRelayStatisticsInfo::addSessionAuthUser()"
            << " getClientEndpointAddrString:"
            << ptr->getClientEndpointAddrString()
            << " getClientEndpointAddrPortString:"
            << ptr->getClientEndpointAddrPortString()
            << " getListenEndpointAddrString:"
            << ptr->getListenEndpointAddrString()
            << " authUser:"
            << (ptr->authUser ? ptr->authUser->id : 0);
        BOOST_ASSERT(n->sessions.get<SessionInfo::ListenClientAddrPortPair>().find(std::make_tuple(
                ptr->getClientEndpointAddrPortString(),
                ptr->getListenEndpointAddrString()
        )) == n->sessions.get<SessionInfo::ListenClientAddrPortPair>().end());
        n->sessions.emplace_back(s);
//        if (auto ns = ptr->getNowServer()) {
//            std::lock_guard lgLM{n->lastUseUpstreamIndexMtx};
//            n->lastUseUpstreamIndex = ns->index;
//        }
    }
}

void TcpRelayStatisticsInfo::updateSessionInfo(std::shared_ptr<TcpRelaySession> s) {
    BOOST_ASSERT(s);
    std::lock_guard lgG{mtx};
    if (s) {
        {
            auto ui = upstreamIndex.find(s->getNowServer()->index);
            if (ui == upstreamIndex.end()) {
                std::lock_guard lg{ui->second->sessionsMtx};
                auto &kp = ui->second->sessions.get<SessionInfo::ListenClientAddrPortPair>();
                auto it = kp.find(std::make_tuple(
                        s->getClientEndpointAddrPortString(),
                        s->getListenEndpointAddrString()
                ));
                if (it != kp.end()) {
                    kp.modify(it, [&](SessionInfo &a) {
                        a.updateTargetInfo(s);
                    });
                }
            }
        }
        {
            auto ci = clientIndex.find(s->getClientEndpointAddrString());
            if (ci == clientIndex.end()) {
                std::lock_guard lg{ci->second->sessionsMtx};
                auto &kp = ci->second->sessions.get<SessionInfo::ListenClientAddrPortPair>();
                auto it = kp.find(std::make_tuple(
                        s->getClientEndpointAddrPortString(),
                        s->getListenEndpointAddrString()
                ));
                if (it != kp.end()) {
                    kp.modify(it, [&](SessionInfo &a) {
                        a.updateTargetInfo(s);
                    });
                }
            }
        }
        {
            auto li = listenIndex.find(s->getListenEndpointAddrString());
            if (li == listenIndex.end()) {
                std::lock_guard lg{li->second->sessionsMtx};
                auto &kp = li->second->sessions.get<SessionInfo::ListenClientAddrPortPair>();
                auto it = kp.find(std::make_tuple(
                        s->getClientEndpointAddrPortString(),
                        s->getListenEndpointAddrString()
                ));
                if (it != kp.end()) {
                    kp.modify(it, [&](SessionInfo &a) {
                        a.updateTargetInfo(s);
                    });
                }
            }
        }
    }
}

std::shared_ptr<TcpRelayStatisticsInfo::Info> TcpRelayStatisticsInfo::getInfo(size_t index) {
    std::lock_guard lg{mtx};
    auto n = upstreamIndex.find(index);
    if (n != upstreamIndex.end()) {
        return n->second;
    } else {
        return {};
    }
}

std::shared_ptr<TcpRelayStatisticsInfo::Info> TcpRelayStatisticsInfo::getInfoClient(const std::string &addr) {
    std::lock_guard lg{mtx};
    auto n = clientIndex.find(addr);
    if (n != clientIndex.end()) {
        return n->second;
    } else {
        return {};
    }
}

std::shared_ptr<TcpRelayStatisticsInfo::Info> TcpRelayStatisticsInfo::getInfoListen(const std::string &addr) {
    std::lock_guard lg{mtx};
    auto n = listenIndex.find(addr);
    if (n != listenIndex.end()) {
        return n->second;
    } else {
        return {};
    }
}

std::shared_ptr<TcpRelayStatisticsInfo::Info> TcpRelayStatisticsInfo::getInfoAuthUser(size_t id) {
    std::lock_guard lg{mtx};
    auto n = authUserIndex.find(id);
    BOOST_ASSERT(n != authUserIndex.end());
    if (n != authUserIndex.end()) {
        return n->second;
    } else {
        return {};
    }
}

void TcpRelayStatisticsInfo::removeExpiredSession(size_t index) {
    std::lock_guard lg{mtx};
    auto p = getInfo(index);
    if (p) {
        p->removeExpiredSession();
    }
}

void TcpRelayStatisticsInfo::removeExpiredSessionClient(const std::string &addr) {
    std::lock_guard lg{mtx};
    auto p = getInfoClient(addr);
    if (p) {
        p->removeExpiredSession();
    }
}

void TcpRelayStatisticsInfo::removeExpiredSessionListen(const std::string &addr) {
    std::lock_guard lg{mtx};
    auto p = getInfoListen(addr);
    if (p) {
        p->removeExpiredSession();
    }
}

void TcpRelayStatisticsInfo::removeExpiredSessionAuthUser(size_t id) {
    std::lock_guard lg{mtx};
    auto p = getInfoAuthUser(id);
    if (p) {
        p->removeExpiredSession();
    }
}

void TcpRelayStatisticsInfo::addByteUp(size_t index, size_t b) {
    std::lock_guard lg{mtx};
    auto p = getInfo(index);
    if (p) {
        p->byteUp += b;
    }
}

void TcpRelayStatisticsInfo::addByteUpClient(const std::string &addr, size_t b) {
    std::lock_guard lg{mtx};
    auto p = getInfoClient(addr);
    if (p) {
        p->byteUp += b;
    }
}

void TcpRelayStatisticsInfo::addByteUpListen(const std::string &addr, size_t b) {
    std::lock_guard lg{mtx};
    auto p = getInfoListen(addr);
    if (p) {
        p->byteUp += b;
    }
}

void TcpRelayStatisticsInfo::addByteUpAuthUser(size_t id, size_t b) {
    std::lock_guard lg{mtx};
    auto p = getInfoAuthUser(id);
    if (p) {
        p->byteUp += b;
    }
}

void TcpRelayStatisticsInfo::addByteDown(size_t index, size_t b) {
    std::lock_guard lg{mtx};
    auto p = getInfo(index);
    if (p) {
        p->byteDown += b;
    }
}

void TcpRelayStatisticsInfo::addByteDownClient(const std::string &addr, size_t b) {
    std::lock_guard lg{mtx};
    auto p = getInfoClient(addr);
    if (p) {
        p->byteDown += b;
    }
}

void TcpRelayStatisticsInfo::addByteDownListen(const std::string &addr, size_t b) {
    std::lock_guard lg{mtx};
    auto p = getInfoListen(addr);
    if (p) {
        p->byteDown += b;
    }
}

void TcpRelayStatisticsInfo::addByteDownAuthUser(size_t id, size_t b) {
    std::lock_guard lg{mtx};
    auto p = getInfoAuthUser(id);
    if (p) {
        p->byteDown += b;
    }
}

void TcpRelayStatisticsInfo::calcByteAll() {
    std::lock_guard lg{mtx};
    for (auto &a: upstreamIndex) {
        a.second->calcByte();
    }
    for (auto &a: clientIndex) {
        a.second->calcByte();
    }
    for (auto &a: listenIndex) {
        a.second->calcByte();
    }
    for (auto &a: authUserIndex) {
        a.second->calcByte();
    }
}

void TcpRelayStatisticsInfo::removeExpiredSessionAll() {
    std::lock_guard lg{mtx};
    for (auto &a: upstreamIndex) {
        a.second->removeExpiredSession();
    }
    for (auto &a: clientIndex) {
        a.second->removeExpiredSession();
    }
    for (auto &a: listenIndex) {
        a.second->removeExpiredSession();
    }
    for (auto &a: authUserIndex) {
        a.second->removeExpiredSession();
    }
}

void TcpRelayStatisticsInfo::closeAllSession(size_t index) {
    std::lock_guard lg{mtx};
    auto p = getInfo(index);
    if (p) {
        p->closeAllSession();
    }
}

void TcpRelayStatisticsInfo::closeAllSessionClient(const std::string &addr) {
    std::lock_guard lg{mtx};
    auto p = getInfoClient(addr);
    if (p) {
        p->closeAllSession();
    }
}

void TcpRelayStatisticsInfo::closeAllSessionListen(const std::string &addr) {
    std::lock_guard lg{mtx};
    auto p = getInfoListen(addr);
    if (p) {
        p->closeAllSession();
    }
}

void TcpRelayStatisticsInfo::closeAllSessionAuthUser(size_t id) {
    std::lock_guard lg{mtx};
    auto p = getInfoAuthUser(id);
    if (p) {
        p->closeAllSession();
    }
}

void TcpRelayStatisticsInfo::connectCountAdd(size_t index) {
    std::lock_guard lg{mtx};
    auto p = getInfo(index);
    if (p) {
        p->connectCountAdd();
    }
}

void TcpRelayStatisticsInfo::connectCountAddClient(const std::string &addr) {
    std::lock_guard lg{mtx};
    auto p = getInfoClient(addr);
    if (p) {
        p->connectCountAdd();
    }
}

void TcpRelayStatisticsInfo::connectCountAddListen(const std::string &addr) {
    std::lock_guard lg{mtx};
    auto p = getInfoListen(addr);
    if (p) {
        p->connectCountAdd();
    }
}

void TcpRelayStatisticsInfo::connectCountAddAuthUser(size_t id) {
    std::lock_guard lg{mtx};
    auto p = getInfoAuthUser(id);
    if (p) {
        p->connectCountAdd();
    }
}

void TcpRelayStatisticsInfo::connectCountSub(size_t index) {
    std::lock_guard lg{mtx};
    auto p = getInfo(index);
    if (p) {
        p->connectCountSub();
    }
}

void TcpRelayStatisticsInfo::connectCountSubClient(const std::string &addr) {
    std::lock_guard lg{mtx};
    auto p = getInfoClient(addr);
    if (p) {
        p->connectCountSub();
    }
}

void TcpRelayStatisticsInfo::connectCountSubListen(const std::string &addr) {
    std::lock_guard lg{mtx};
    auto p = getInfoListen(addr);
    if (p) {
        p->connectCountSub();
    }
}

void TcpRelayStatisticsInfo::connectCountSubAuthUser(size_t id) {
    std::lock_guard lg{mtx};
    auto p = getInfoAuthUser(id);
    if (p) {
        p->connectCountSub();
    }
}

std::map<size_t, std::shared_ptr<TcpRelayStatisticsInfo::Info>> TcpRelayStatisticsInfo::getUpstreamIndex() {
    std::lock_guard lg{mtx};
    return decltype(upstreamIndex){upstreamIndex.begin(), upstreamIndex.end()};
}

std::map<std::string, std::shared_ptr<TcpRelayStatisticsInfo::Info>> TcpRelayStatisticsInfo::getClientIndex() {
    std::lock_guard lg{mtx};
    return decltype(clientIndex){clientIndex.begin(), clientIndex.end()};
}

std::map<std::string, std::shared_ptr<TcpRelayStatisticsInfo::Info>> TcpRelayStatisticsInfo::getListenIndex() {
    std::lock_guard lg{mtx};
    return decltype(listenIndex){listenIndex.begin(), listenIndex.end()};
}

std::map<size_t, std::shared_ptr<TcpRelayStatisticsInfo::Info>> TcpRelayStatisticsInfo::getAuthUserIndex() {
    std::lock_guard lg{mtx};
    return decltype(authUserIndex){authUserIndex.begin(), authUserIndex.end()};
}
