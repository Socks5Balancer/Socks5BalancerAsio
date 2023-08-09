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

#include "AuthClientManager.h"

void AuthClientManager::initUserInfo() {
    // empty now
    for (const auto &a: configLoader->config.authClientInfo) {
        userPwd.insert(std::make_pair(a.user, a.pwd));
        base64AuthStringSet.insert(a.base64AuthString);
    }
}

bool AuthClientManager::needAuth() {
    return !userPwd.empty();
}

bool AuthClientManager::checkAuth(const std::string_view &user, const std::string_view &pwd) {
    auto it = userPwd.equal_range(std::string{user});
    if (it.first != userPwd.end()) {
        for (auto i = it.first; i != it.second; ++i) {
//                std::cout << i->first << ": " << i->second << '\n';
            if (i->second == pwd) {
                return true;
            }
        }
        // cannot find a match pwd
        return false;
    } else {
        // cannot find the user
        return false;
    }
}

bool AuthClientManager::checkAuth_Base64AuthString(const std::string_view &base64AuthString) {
    auto it = base64AuthStringSet.find(std::string{base64AuthString});
    return it != base64AuthStringSet.end();
}
