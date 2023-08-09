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

#include "ConfigLoader.h"


class AuthClientManager : public std::enable_shared_from_this<AuthClientManager> {
public:
    std::shared_ptr<ConfigLoader> configLoader;

    std::multimap<std::string, std::string> userPwd;
    std::set<std::string> base64AuthStringSet;

public:
    AuthClientManager(std::shared_ptr<ConfigLoader> configLoader) : configLoader(std::move(configLoader)) {
        initUserInfo();
    }

public:

    void initUserInfo();

    bool needAuth();

    bool checkAuth(const std::string_view &user, const std::string_view &pwd);

    bool checkAuth_Base64AuthString(const std::string_view &base64AuthString);

};


#endif //SOCKS5BALANCERASIO_AUTHCLIENTMANAGER_H
