#pragma once
#include <cstring>
#include <system_error>
#include <vector>
#include <string>

#include <unistd.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "host_handle.hpp"

namespace asa {
namespace aix {

inline std::string get_os_info(std::error_code& ec) {
    ec.clear();
    return "Aix";
};

inline std::string get_hostname(std::error_code& ec) {
    ec.clear();
    char hostname[1024]{};
    auto ret = gethostname(hostname, 1024);
    if (ret == -1) {
        ec = std::error_code(errno, std::system_category());
        return {};
    }
    return std::string(hostname);
}

inline cpu_occupy get_cpu_occupy(std::error_code& ec) {
    ec.clear();
    return {};
}

inline int32_t calculate_cpu_usage(const cpu_occupy&, const cpu_occupy&) {
    return {};
}

inline int32_t get_memory_usage(std::error_code& ec) {
    ec.clear();
    return {};
}


inline uint32_t get_network_card_speed(const std::string & card_name) {
    if (card_name.compare(0, 2, "lo") == 0) {
        return 1000 * 40;
    }
    constexpr char temp[] = R"(entstat -d %s | grep Running | grep -o '[0-9]\+')";
    char cmd[128]{};
    sprintf(cmd, temp, card_name.data());

    auto fd = popen(cmd, "r");
    if (fd == nullptr) {
        return 1000;
    }
    char buf[64] = { 0 };
    fgets(buf, sizeof(buf), fd);
    pclose(fd);
    auto speed = atoi(buf);
    return speed == 0 ? 1000 : speed;
};

inline bool is_physics(const std::string&) {
    return {};
}

inline network_card_t get_network_card(std::error_code& ec) {
    ec.clear();
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock == -1) {
        ec = std::error_code(errno, std::system_category());
        return {};
    }
    auto closer = std::shared_ptr<char>(new char, [sock](char* p) {delete p;  close(sock); });

    ifconf ifc{};
    char buf[2048]{};
    ifc.ifc_len = sizeof(buf);
    ifc.ifc_buf = (caddr_t)buf;
    if (ioctl(sock, SIOCGIFCONF, &ifc) == -1) {
        ec = std::error_code(errno, std::system_category());
        return {};
    }

    network_card_t cards;
    auto it = ifc.ifc_req;
    auto end = it + (ifc.ifc_len / sizeof(struct ifreq));
    for (; it != end; ++it) {
        auto if_idx = if_nametoindex(it->ifr_name);
        std::string card_name = it->ifr_name;
        if (card_name.empty() || if_idx == 0) {
            continue;
        }
   
        bool is_down = false;
        ifreq ifr{};
        strcpy(ifr.ifr_name, it->ifr_name);
        if (ioctl(sock, SIOCGIFFLAGS, &ifr) == 0) {
            if (ifr.ifr_flags & IFF_UP) {
                is_down = false;
            }
            else if (ifr.ifr_flags & IFF_LOOPBACK) {
                is_down = false;
            }
            else {
                is_down = true;
            }
        }

        auto it = cards.find(card_name);
        if (it == cards.end()) {
            networkcard card{};
            card.ifindex = if_idx;
            card.is_down = is_down;
            card.is_physics = is_physics(card_name);
            card.real_name = card_name;
            card.friend_name = card_name;
            card.desc = card_name;
            card.receive_speed = get_network_card_speed(card_name);
            card.transmit_speed = card.receive_speed;
            cards.emplace(card_name, std::move(card));
            it = cards.find(card_name);
        }

        if (ioctl(sock, SIOCGIFADDR, &ifr) == 0) {
            if (ifr.ifr_addr.sa_family == AF_INET) {
                char if_ipv4[16]{};
                snprintf(if_ipv4, sizeof(if_ipv4), "%s",
                    inet_ntoa(((struct sockaddr_in*)&(ifr.ifr_addr))->sin_addr));
                it->second.ipv4.emplace(if_ipv4);
            }
        }
    }
    return cards;
}

template<typename C>
inline card_flow get_network_card_flow(C&&, std::error_code& ec) {
    ec.clear();
    return {};
}

inline disk_info get_disk_info(std::string_view, std::error_code& ec) {
    ec.clear();
    return {};
}

inline auto get_toute_table(std::error_code& ec) {
    ec.clear();
    std::vector<std::vector<std::string>> tables;
    return tables;
}

}
}