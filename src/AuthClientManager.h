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

#ifndef SOCKS5BALANCERASIO_AUTHCLIENTMANAGER_H
#define SOCKS5BALANCERASIO_AUTHCLIENTMANAGER_H

#ifdef MSVC
#pragma once
#endif


#include <memory>
#include <iostream>
#include <string>
#include <map>
#include <set>
#include <functional>
#include <utility>
#include <atomic>

#include "ConfigLoader.h"


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


class AuthClientManager : public std::enable_shared_from_this<AuthClientManager> {
public:

    struct AuthUser : public std::enable_shared_from_this<AuthUser> {
        struct ID {
        };
        struct USER {
        };
        struct PWD {
        };
        struct USER_PWD {
        };
        struct BASE64 {
        };
        const size_t id;
        const std::string user;
        const std::string pwd;
        // for http aut speedup
        const std::string base64;

        AuthUser(
                const size_t &id,
                std::string user,
                std::string pwd,
                std::string base64
        ) : id(id),
            user(std::move(user)),
            pwd(std::move(pwd)),
            base64(std::move(base64)) {}

        AuthUser(const AuthUser &o) = default;

        std::strong_ordering operator<=>(const AuthUser &o) const {
            // https://zh.cppreference.com/w/cpp/language/default_comparisons
            if ((id <=> o.id) != std::strong_ordering::equal) {
                return id <=> o.id;
            } else if ((base64 <=> o.base64) != std::strong_ordering::equal) {
                return base64 <=> o.base64;
            } else if ((user <=> o.user) != std::strong_ordering::equal) {
                return user <=> o.user;
            } else if ((pwd <=> o.pwd) != std::strong_ordering::equal) {
                return pwd <=> o.pwd;
            }
            return std::strong_ordering::equal;
        }

    };

    using AuthInfoContainer = boost::multi_index_container<
            std::shared_ptr<AuthUser>,
            boost::multi_index::indexed_by<
                    boost::multi_index::sequenced<>,
                    boost::multi_index::ordered_unique<
                            boost::multi_index::identity<AuthUser>
                    >,
                    boost::multi_index::hashed_unique<
                            boost::multi_index::tag<AuthUser::ID>,
                            boost::multi_index::member<AuthUser, const size_t, &AuthUser::id>
                    >,
                    boost::multi_index::hashed_non_unique<
                            boost::multi_index::tag<AuthUser::USER>,
                            boost::multi_index::member<AuthUser, const std::string, &AuthUser::user>
                    >,
                    boost::multi_index::hashed_non_unique<
                            boost::multi_index::tag<AuthUser::PWD>,
                            boost::multi_index::member<AuthUser, const std::string, &AuthUser::pwd>
                    >,
                    boost::multi_index::hashed_unique<
                            boost::multi_index::tag<AuthUser::USER_PWD>,
                            boost::multi_index::composite_key<
                                    AuthUser,
                                    boost::multi_index::member<AuthUser, const std::string, &AuthUser::user>,
                                    boost::multi_index::member<AuthUser, const std::string, &AuthUser::pwd>
                            >
                    >,
                    boost::multi_index::hashed_unique<
                            boost::multi_index::tag<AuthUser::BASE64>,
                            boost::multi_index::member<AuthUser, const std::string, &AuthUser::base64>
                    >,
                    boost::multi_index::random_access<>
            >
    >;

public:
    std::shared_ptr<ConfigLoader> configLoader;

    std::atomic_size_t lastId{0};

    AuthInfoContainer authInfo;

public:
    AuthClientManager(std::shared_ptr<ConfigLoader> configLoader) : configLoader(std::move(configLoader)) {
        initUserInfo();
    }

public:

    void initUserInfo();

    bool needAuth();

    std::shared_ptr<AuthClientManager::AuthUser> checkAuth(const std::string_view &user, const std::string_view &pwd);

    std::shared_ptr<AuthClientManager::AuthUser> checkAuth_Base64AuthString(const std::string_view &base64AuthString);

    std::shared_ptr<AuthClientManager::AuthUser> getById(size_t id);

};


#endif //SOCKS5BALANCERASIO_AUTHCLIENTMANAGER_H
