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

#include <memory>
#include <string>
#include <utility>
#include <list>
#include <map>
#include <atomic>
#include <mutex>
#include "ConfigRuleEnum.h"

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/tag.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/identity.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/sequenced_index.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/random_access_index.hpp>
#include <boost/multi_index/composite_key.hpp>
#include <boost/multi_index/key.hpp>


class TcpRelaySession;

class TcpRelayStatisticsInfo : public std::enable_shared_from_this<TcpRelayStatisticsInfo> {
public:

    struct SessionInfo {
        struct UpstreamIndex {
        };
        struct ClientAddrPort {
        };
        struct ListenAddr {
        };
        struct ListenClientAddrPortPair {
        };
        struct RawPtr {
        };

        const size_t upstreamIndex;
        const std::string clientEndpointAddrPortString;
        const std::string listenEndpointAddrString;

        const TcpRelaySession *rawPtr{nullptr};
        const std::weak_ptr<TcpRelaySession> ptr;

        const uint64_t startTime;

        explicit SessionInfo(const std::weak_ptr<TcpRelaySession> &s);

        explicit SessionInfo(const std::shared_ptr<TcpRelaySession> &s);

        SessionInfo(const SessionInfo &o) = default;

        std::strong_ordering operator<=>(const SessionInfo &o) const {
            // https://zh.cppreference.com/w/cpp/language/default_comparisons
            if ((upstreamIndex <=> o.upstreamIndex) != std::strong_ordering::equal) {
                return upstreamIndex <=> o.upstreamIndex;
            } else if ((clientEndpointAddrPortString <=> o.clientEndpointAddrPortString) !=
                       std::strong_ordering::equal) {
                return clientEndpointAddrPortString <=> o.clientEndpointAddrPortString;
            } else if ((listenEndpointAddrString <=> o.listenEndpointAddrString) != std::strong_ordering::equal) {
                return listenEndpointAddrString <=> o.listenEndpointAddrString;
            } else if ((rawPtr <=> o.rawPtr) != std::strong_ordering::equal) {
                return rawPtr <=> o.rawPtr;
            }
            return std::strong_ordering::equal;
        }

        std::string host;
        uint16_t post{};
        std::string targetEndpointAddrString;

        size_t authUserId{0};

    protected:
        friend TcpRelayStatisticsInfo;

        void updateTargetInfo(const std::shared_ptr<TcpRelaySession> &s);
    };

    using SessionInfoContainer = boost::multi_index_container<
//            boost::shared_ptr<SessionInfo>,
            SessionInfo,
            boost::multi_index::indexed_by<
                    boost::multi_index::sequenced<>,
                    boost::multi_index::ordered_unique<
                            boost::multi_index::identity<SessionInfo>
                    >,
                    boost::multi_index::hashed_non_unique<
                            boost::multi_index::tag<SessionInfo::UpstreamIndex>,
                            boost::multi_index::member<SessionInfo, const size_t, &SessionInfo::upstreamIndex>
                    >,
                    boost::multi_index::hashed_non_unique<
                            boost::multi_index::tag<SessionInfo::ClientAddrPort>,
                            boost::multi_index::member<SessionInfo, const std::string, &SessionInfo::clientEndpointAddrPortString>
                    >,
                    boost::multi_index::hashed_non_unique<
                            boost::multi_index::tag<SessionInfo::ListenAddr>,
                            boost::multi_index::member<SessionInfo, const std::string, &SessionInfo::listenEndpointAddrString>
                    >,
                    boost::multi_index::hashed_unique<
                            boost::multi_index::tag<SessionInfo::ListenClientAddrPortPair>,
                            boost::multi_index::composite_key<
                                    SessionInfo,
                                    boost::multi_index::member<SessionInfo, const std::string, &SessionInfo::clientEndpointAddrPortString>,
                                    boost::multi_index::member<SessionInfo, const std::string, &SessionInfo::listenEndpointAddrString>
                            >
                    >,
                    boost::multi_index::hashed_unique<
                            boost::multi_index::tag<SessionInfo::RawPtr>,
                            boost::multi_index::member<SessionInfo, const TcpRelaySession *, &SessionInfo::rawPtr>
                    >,
                    boost::multi_index::random_access<>
            >/*,
            SessionInfo*/
    >;

    struct Info : public std::enable_shared_from_this<Info> {

        std::recursive_mutex sessionsMtx;
        TcpRelayStatisticsInfo::SessionInfoContainer sessions{};

        std::atomic_size_t byteUp = 0;
        std::atomic_size_t byteDown = 0;
        std::atomic_size_t byteUpLast = 0;
        std::atomic_size_t byteDownLast = 0;
        std::atomic_size_t byteUpChange = 0;
        std::atomic_size_t byteDownChange = 0;
        std::atomic_size_t byteUpChangeMax = 0;
        std::atomic_size_t byteDownChangeMax = 0;

        std::atomic_size_t connectCount{0};

        RuleEnum rule = RuleEnum::inherit;
        std::recursive_mutex lastUseUpstreamIndexMtx;
        size_t lastUseUpstreamIndex = 0;

        std::recursive_mutex calcByteMtx;

    protected:
        friend TcpRelayStatisticsInfo;

        void removeExpiredSession();

        void calcByte();

        void connectCountAdd();

        void connectCountSub();

    public:

        void closeAllSession();

        size_t calcSessionsNumber();
    };

private:
    std::recursive_mutex mtx;

    // upstreamIndex
    std::map<size_t, std::shared_ptr<Info>> upstreamIndex;
    // clientEndpointAddrString "ip"
    std::map<std::string, std::shared_ptr<Info>> clientIndex;
    // listenEndpointAddrString "ip:port"
    std::map<std::string, std::shared_ptr<Info>> listenIndex;
    // authUser id
    std::map<size_t, std::shared_ptr<Info>> authUserIndex;

public:
    std::atomic_size_t lastConnectServerIndex{0};

public:
    // nowServer->index
    std::map<size_t, std::shared_ptr<Info>> getUpstreamIndex();

    // ClientEndpointAddrString : (127.0.0.1)
    std::map<std::string, std::shared_ptr<Info>> getClientIndex();

    // ListenEndpointAddrString : (127.0.0.1:661133)
    std::map<std::string, std::shared_ptr<Info>> getListenIndex();

    std::map<size_t, std::shared_ptr<Info>> getAuthUserIndex();

public:
    void addSession(size_t index, const std::shared_ptr<TcpRelaySession> &s);

    void addSessionClient(const std::shared_ptr<TcpRelaySession> &s);

    void addSessionListen(const std::shared_ptr<TcpRelaySession> &s);

    void addSessionAuthUser(const std::shared_ptr<TcpRelaySession> &s);

    void updateSessionInfo(std::shared_ptr<TcpRelaySession> s);

    std::shared_ptr<Info> getInfo(size_t index);

    std::shared_ptr<Info> getInfoClient(const std::string &addr);

    std::shared_ptr<Info> getInfoListen(const std::string &addr);

    std::shared_ptr<Info> getInfoAuthUser(size_t id);

    void removeExpiredSession(size_t index);

    void removeExpiredSessionClient(const std::string &addr);

    void removeExpiredSessionListen(const std::string &addr);

    void removeExpiredSessionAuthUser(size_t id);

    void addByteUp(size_t index, size_t b);

    void addByteUpClient(const std::string &addr, size_t b);

    void addByteUpListen(const std::string &addr, size_t b);

    void addByteUpAuthUser(size_t id, size_t b);

    void addByteDown(size_t index, size_t b);

    void addByteDownClient(const std::string &addr, size_t b);

    void addByteDownListen(const std::string &addr, size_t b);

    void addByteDownAuthUser(size_t id, size_t b);

    void connectCountAdd(size_t index);

    void connectCountAddClient(const std::string &addr);

    void connectCountAddListen(const std::string &addr);

    void connectCountAddAuthUser(size_t id);

    void connectCountSub(size_t index);

    void connectCountSubClient(const std::string &addr);

    void connectCountSubListen(const std::string &addr);

    void connectCountSubAuthUser(size_t id);

    void calcByteAll();

    void removeExpiredSessionAll();

    void closeAllSession(size_t index);

    void closeAllSessionClient(const std::string &addr);

    void closeAllSessionListen(const std::string &addr);

    void closeAllSessionAuthUser(size_t id);

};


#endif //SOCKS5BALANCERASIO_TCPRELAYSTATISTICSINFO_H
