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

#include "ConfigLoader.h"

RuleEnum string2RuleEnum(std::string s) {
    if ("loop" == s) {
        return RuleEnum::loop;
    }
    if ("random" == s) {
        return RuleEnum::random;
    }
    if ("one_by_one" == s) {
        return RuleEnum::one_by_one;
    }
    if ("change_by_time" == s) {
        return RuleEnum::change_by_time;
    }
    return RuleEnum::random;
}

std::string ruleEnum2string(RuleEnum r) {
    switch (r) {
        case RuleEnum::loop:
            return "loop";
        case RuleEnum::random:
            return "random";
        case RuleEnum::one_by_one:
            return "one_by_one";
        case RuleEnum::change_by_time:
            return "change_by_time";
        default:
            return "random";
    }
}
