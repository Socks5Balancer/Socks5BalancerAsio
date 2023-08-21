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
        authInfo.emplace_back(std::make_shared<AuthUser>(++lastId, a.user, a.pwd, a.base64AuthString));
    }
}

bool AuthClientManager::needAuth() {
    return !authInfo.empty();
}

std::shared_ptr<AuthClientManager::AuthUser>
AuthClientManager::checkAuth(const std::string_view &user, const std::string_view &pwd) {
    auto &userPwd = authInfo.get<AuthUser::USER_PWD>();
    auto it = userPwd.find(std::make_tuple(std::string{user}, std::string{pwd}));
    if (it != userPwd.end()) {
        return *it;
    } else {
        return {};
    }
}

std::shared_ptr<AuthClientManager::AuthUser>
AuthClientManager::checkAuth_Base64AuthString(const std::string_view &base64AuthString) {
    auto &base64AuthStringSet = authInfo.get<AuthUser::BASE64>();
    auto it = base64AuthStringSet.find(std::string{base64AuthString});
    if (it != base64AuthStringSet.end()) {
        return *it;
    } else {
        return {};
    }
}

std::shared_ptr<AuthClientManager::AuthUser> AuthClientManager::getById(size_t id) {
    auto &idL = authInfo.get<AuthUser::ID>();
    auto it = idL.find(id);
    if (it != idL.end()) {
        return *it;
    } else {
        return {};
    }
}
