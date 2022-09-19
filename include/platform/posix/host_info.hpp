#pragma once
#include <string>
#include <cstdint>
#include <codecvt>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <filesystem>

#include <unistd.h>
#include <string.h>
#include <math.h>
#include <net/if.h>
#include <ifaddrs.h>  
#include <arpa/inet.h>

#include "host_handle.hpp"

namespace asa {
namespace posix {

inline std::string get_os_info_internal( const std::string& from, const std::string& keyword)
{
    std::string os_info = "unknown system";
    FILE* fd = fopen(from.data(), "r");
    if (fd == nullptr) {
        return os_info;
    }

    char buf[256] = { 0 };
    while (fgets(buf, sizeof(buf), fd)) {
        std::string info = buf;
        auto pos = info.find(keyword);
        if (pos == std::string::npos) {
            continue;
        }

        if (keyword == "PRETTY_NAME") { //PRETTY_NAME="CentOS Linux 7 (Core)"
            pos = info.find_last_of('"');
            os_info = info.substr(13, pos - 13);
        }
        else { //CentOS release 6.10 (Final)
            os_info = std::move(info);
        }
        break;
    }
    fclose(fd);
    return os_info;
};

inline std::string get_os_info() {
    if (std::filesystem::exists("/etc/os-release")) {
        return get_os_info_internal("/etc/os-release", "PRETTY_NAME");
    }
    else {  //this maybe centos6
        return get_os_info_internal("/etc/issue", "CentOS");
    }
};

inline std::string get_hostname() {
    char hostname[1024]{};
    gethostname(hostname, 1024);
    return std::string(hostname);
}

inline bool get_cpu_occupy(cpu_occupy& occupy) {
    FILE* fd = fopen("/proc/stat", "r");
    if (fd == nullptr) {
        return false;
    }
    char buff[256] = { 0 };
    while (fgets(buff, sizeof(buff), fd)) {
        sscanf(buff, "%s %llu %llu %llu %llu",
            occupy.name, &occupy.user, &occupy.nice, &occupy.system, &occupy.idle);

        if (!strcmp("cpu", occupy.name)) {
            break;
        }
        memset(buff, 0, sizeof(buff));
    }
    fclose(fd);
    return true;
}

inline int32_t calculate_cpu_usage(const cpu_occupy& pre, const cpu_occupy& now)
{
    uint64_t pre_total = pre.user + pre.nice + pre.system + pre.idle;
    uint64_t now_total = now.user + now.nice + now.system + now.idle;
    auto total_detal = now_total - pre_total;
    auto used_detal = now.user + pre.system - pre.user - pre.system;
   
    // +1 for avoiding usage = 0
    int usage = (int32_t)ceil((double)(used_detal + 1) * 100 / (double)(total_detal));
    return usage;
}

inline int32_t get_memory_usage() {
    memory_info meminfo{};
    FILE* fp = fopen("/proc/meminfo", "r");
    if (fp == nullptr) {
        return -1;
    }
    char name[256] = { 0 };
    char buf[256] = { 0 };
    char unit[32] = { 0 };
    uint16_t ok = 0;
    while (fgets(buf, sizeof(buf), fp)) {
        uint64_t data;
        sscanf(buf, "%s %llu %s", name, &data, unit);
        if (!strcmp("MemTotal:", name)) {
            meminfo.total = data;
            ok = ok | (uint16_t)0x0001;
        }
        else if (!strcmp("MemFree:", name)) {
            meminfo.free = data;
            ok = ok | (uint16_t)0x0010;
        }
        else if (!strcmp("Buffers:", name)) {
            meminfo.buffers = data;
            ok = ok | (uint16_t)0x0100;
        }
        else if (!strcmp("Cached:", name)) {
            meminfo.cached = data;
            ok = ok | (uint16_t)0x1000;
        }

        if (ok == (uint16_t)0x1111) {
            break; //find all key
        }
        memset(name, 0, sizeof(name));
        memset(buf, 0, sizeof(buf));
        memset(unit, 0, sizeof(unit));
    }
    fclose(fp);

    auto used = meminfo.total - meminfo.free - meminfo.buffers - meminfo.cached;
    int mem_usage = (int)ceil((double)used * 100 / (double)meminfo.total);
    return mem_usage;
}

inline card_state get_network_card_state(const std::string& card_name) {
    auto file = "/sys/class/net/" + card_name + "/operstate";
    FILE* fp = fopen(file.data(), "r");
    if (fp == nullptr) {
        return card_state::unknown;
    }
    char buf[64] = { 0 };
    fgets(buf, sizeof(buf), fp);
    fclose(fp);

    if (strncasecmp(buf, "up", 2) == 0) {
        return card_state::up;
    }
    if (strncasecmp(buf, "down", 4) == 0) {
        return card_state::down;
    }
    return card_state::unknown;
};

inline uint32_t get_network_card_speed(const std::string& card_name) {
    if (card_name == "lo") {
        return 1000 * 40;
    }

    auto file = "/sys/class/net/" + card_name + "/speed";
    FILE* fp = fopen(file.data(), "r");
    if (fp == nullptr) {
        return 0;
    }
    char buf[64] = { 0 };
    fgets(buf, sizeof(buf), fp);
    fclose(fp);
    return atoi(buf);
};

inline network_card_t get_network_card() {
    ifaddrs* ifList{};
    if (getifaddrs(&ifList) < 0) {
        return {};
    }

    network_card_t cards;
    for (auto ifa = ifList; ifa != nullptr; ifa = ifa->ifa_next) {
        std::string name = ifa->ifa_name;
        if (name == "docker0" ||
            (name.length() > 4 && name.compare(0, 4, "veth") == 0) ||
            (name.length() > 3 && name.compare(0, 3, "br-") == 0))
        {
            continue; //remove docker interface
        }

        auto it = cards.find(name);
        if (it == cards.end()) {
            networkcard card{};
            card.is_down = get_network_card_state(name) == card_state::down ? true : false;
            card.real_name = name;
            card.friend_name = name;
            card.desc = name;
            card.recive_speed = get_network_card_speed(name);
            card.transmit_speed = card.recive_speed;
            cards.emplace(name, std::move(card));
        }

        if (ifa->ifa_addr->sa_family == AF_INET) {
            char address_ip[INET_ADDRSTRLEN]{};
            auto sin = (struct sockaddr_in*)ifa->ifa_addr;
            it->second.ipv4.emplace(inet_ntop(AF_INET, &sin->sin_addr, address_ip, INET_ADDRSTRLEN));
        }
        else if (ifa->ifa_addr->sa_family == AF_INET6) {
            char address_ip[INET6_ADDRSTRLEN]{};
            auto sin6 = (struct sockaddr_in6*)ifa->ifa_addr;
            it->second.ipv6.emplace(inet_ntop(AF_INET6, &sin6->sin6_addr, address_ip, INET6_ADDRSTRLEN));
        }
    }

    freeifaddrs(ifList);
    return cards;
}

template<typename C>
inline card_flow get_network_card_flow(C&& c)
{
    FILE* fp = fopen("/proc/net/dev", "r");
    if (fp == nullptr) {
        return {};
    }
    char buf[1024] = { 0 };
    char interface[128]{};
    card_flow flow;
    while (fgets(buf, sizeof(buf), fp) != nullptr) {
        uint64_t receive_bytes = 0;
        uint64_t receive_packets, receive_errs, receive_drop,
            receive_fifo, receive_frame, receive_compressed, receive_multicast;

        uint64_t transmit_bytes = 0;
        uint64_t transmit_packets, transmit_errs, transmit_drop,
            transmit_fifo, transmit_colls, transmit_carrier, transmit_compressed;

        sscanf(buf, "%s %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu",
            interface,
            &receive_bytes, &receive_packets, &receive_errs, &receive_drop,
            &receive_fifo, &receive_frame, &receive_compressed, &receive_multicast,
            &transmit_bytes, &transmit_packets, &transmit_errs, &transmit_drop,
            &transmit_fifo, &transmit_colls, &transmit_carrier, &transmit_compressed);

        std::string name{ interface, strlen(interface) - 1 };
        auto it = c.find(name);
        if (it != c.end()) {
            flow.emplace(std::move(name), std::pair{ receive_bytes ,transmit_bytes });
        }
        memset(buf, 0, sizeof(buf));
        memset(interface, 0, sizeof(interface));
    }
    fclose(fp);
    return flow;
}


}
}
