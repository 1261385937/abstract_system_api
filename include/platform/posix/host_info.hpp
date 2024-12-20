#pragma once
#include <string>
#include <cstdint>
#include <codecvt>
#include <vector>
#include <set>
#include <unordered_set>
#include <unordered_map>
#include <filesystem>
#include <system_error>
#include <future>
#include <tuple>

#include <unistd.h>
#include <string.h>
#include <math.h>
#include <net/if.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <sys/statfs.h> 
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <linux/types.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>

#include "host_handle.hpp"

#if __GLIBC__ == 2 && __GLIBC_MINOR__ < 14
#include <sched.h>
#include "syscall.h"
#ifdef SYS_setns
inline auto setns(int fd, int nstype) {
    return syscall(SYS_setns, fd, nstype);
}
#else
inline int setns(int, int) {
    return -1;
}
#endif
#endif

namespace asa {
namespace posix {

inline std::string get_os_info_internal(
    const std::string& from, const std::string& keyword, std::error_code& ec)
{
    ec.clear();
    std::string os_info = "other";
    FILE* fd = fopen(from.data(), "r");
    if (fd == nullptr) {
        ec = std::error_code(errno, std::system_category());
        return os_info;
    }

    char buf[256] = { 0 };
    while (fgets(buf, sizeof(buf), fd)) {
        if (keyword == "PRETTY_NAME") { //PRETTY_NAME="CentOS Linux 7 (Core)"
            std::string info = buf;
            auto pos = info.find(keyword);
            if (pos == std::string::npos) {
                continue;
            }

            pos = info.find_last_of('"');
            os_info = info.substr(13, pos - 13);
            break;
        }
        else { //CentOS release 6.10 (Final)
            os_info = buf;
            break;
        }
    }
    fclose(fd);
    return os_info;
};

inline std::string get_os_info(std::error_code& ec) {
    ec.clear();
    if (std::filesystem::exists("/etc/os-release")) {
        return get_os_info_internal("/etc/os-release", "PRETTY_NAME", ec);
    }
    else {  //this maybe centos6
        return get_os_info_internal("/etc/issue", "dummy", ec);
    }
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
    FILE* fd = fopen("/proc/stat", "r");
    if (fd == nullptr) {
        ec = std::error_code(errno, std::system_category());
        return {};
    }
    cpu_occupy occupy{};
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
    return occupy;
}

inline int32_t calculate_cpu_usage(const cpu_occupy& pre, const cpu_occupy& now)
{
    uint64_t pre_total = pre.user + pre.nice + pre.system + pre.idle;
    uint64_t now_total = now.user + now.nice + now.system + now.idle;
    auto total_detal = now_total - pre_total;
    auto used_detal = now.user + pre.system - pre.user - pre.system;
    if (total_detal == 0) {
        return {};
    }

    int usage = (int32_t)ceil((double)(used_detal) * 100 / (double)(total_detal));
    return usage;
}

inline int32_t get_memory_usage(std::error_code& ec) {
    ec.clear();
    memory_info meminfo{};
    FILE* fp = fopen("/proc/meminfo", "r");
    if (fp == nullptr) {
        ec = std::error_code(errno, std::system_category());
        return {};
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
    if (meminfo.total == 0) {
        return {};
    }
    int mem_usage = (int)ceil((double)used * 100 / (double)meminfo.total);
    return mem_usage;
}

inline card_state get_network_card_state(const std::string& card_name, std::error_code& ec) {
    ec.clear();
    auto file = "/sys/class/net/" + card_name + "/operstate";
    FILE* fp = fopen(file.data(), "r");
    if (fp == nullptr) {
        ec = std::error_code(errno, std::system_category());
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

inline uint32_t get_network_card_speed(const std::string& card_name, std::error_code&) {
    //ec.clear();
    if (card_name == "lo") {
        return 1000 * 40;
    }

    auto file = "/sys/class/net/" + card_name + "/speed";
    FILE* fp = fopen(file.data(), "r");
    if (fp == nullptr) {
        //ec = std::error_code(errno, std::system_category());
        return 1000 * 10;
    }
    char buf[64] = { 0 };
    fgets(buf, sizeof(buf), fp);
    fclose(fp);
    return atoi(buf);
};

inline auto get_virtual_network_card() {
    std::set<std::string> virtual_cards;
    if (!std::filesystem::exists("/sys/devices/virtual/net/")) {
        return virtual_cards;
    }

    std::filesystem::directory_iterator di("/sys/devices/virtual/net/");
    for (const auto& entry : di) {
        virtual_cards.emplace(entry.path().filename());
    }
    return virtual_cards;
}

inline bool is_physics(const std::string& network_card_name) {
    auto virtual_cards = get_virtual_network_card();
    if (auto it = virtual_cards.find(network_card_name); it != virtual_cards.end()) {
        return false;
    }
    return true;
}

inline auto get_route_table(std::error_code& ec) {
    ec.clear();
    std::vector<std::vector<std::string>> tables;
    FILE* fp = fopen("/proc/net/route", "r");
    if (fp == nullptr) {
        ec = std::error_code(errno, std::system_category());
        return tables;
    }

    constexpr size_t count = 11;
    char buf[1024]{};
    char name[count][32]{};
    fgets(buf, sizeof(buf), fp);
    sscanf(buf, "%s %s %s %s %s %s %s %s %s %s %s", name[0], name[1], name[2],
           name[3], name[4], name[5], name[6], name[7], name[8], name[9], name[10]);
    std::vector<std::string> table;
    table.reserve(count);
    for (size_t i = 0; i < count; i++) {
        table.emplace_back(name[i]);
    }
    tables.emplace_back(std::move(table));

    while (fgets(buf, sizeof(buf), fp)) {
        char Iface[128]{};
        uint32_t Destination = 0;
        uint32_t Gateway = 0;
        char Flags[128]{};
        char RefCnt[128]{};
        char Use[128]{};
        char Metric[128]{};
        uint32_t Mask = 0;
        char MTU[128]{};
        char Window[128]{};
        char IRTT[128]{};
        sscanf(buf, "%s %X %X %s %s %s %s %X %s %s %s",
               Iface, &Destination, &Gateway, Flags, RefCnt, Use, Metric, &Mask, MTU, Window, IRTT);

        table.emplace_back(Iface);
        struct in_addr inAddr {};
        inAddr.s_addr = Destination;
        table.emplace_back(inet_ntoa(inAddr));
        inAddr.s_addr = Gateway;
        table.emplace_back(inet_ntoa(inAddr));
        table.emplace_back(Flags);
        table.emplace_back(RefCnt);
        table.emplace_back(Use);
        table.emplace_back(Metric);
        inAddr.s_addr = Mask;
        table.emplace_back(inet_ntoa(inAddr));
        table.emplace_back(MTU);
        table.emplace_back(Window);
        table.emplace_back(IRTT);
        tables.emplace_back(std::move(table));

        memset(buf, 0, sizeof(buf));
    }
    fclose(fp);
    return tables;
}

inline auto get_route_table_ipv4() {
    std::unordered_map<std::string, std::string> tables;
    FILE* fp = fopen("/proc/net/route", "r");
    if (fp == nullptr) {
        return tables;
    }

    auto virtual_cards = get_virtual_network_card();
    char interface[128]{};
    char buf[1024]{};
    uint32_t dest = 0;
    fgets(buf, sizeof(buf), fp);
    while (fgets(buf, sizeof(buf), fp)) {
        sscanf(buf, "%s %X", interface, &dest);
        size_t i = 0;
        while (interface[i] == ' ') {
            i++;
            continue;
        }
        std::string name{ interface + i, strlen(interface + i) };
        if (virtual_cards.find(name) == virtual_cards.end()) { //physics card do not need
            continue;
        }

        struct in_addr inAddr {};
        inAddr.s_addr = dest;
        tables.emplace(name, inet_ntoa(inAddr)); //calico ip just has one 
        memset(buf, 0, sizeof(buf));
        memset(interface, 0, sizeof(interface));
    }
    fclose(fp);
    return tables;
}

inline auto get_network_card_iflink(const std::string& name) {
    auto file_name = "/sys/class/net/" + name + "/iflink";
    FILE* fp = fopen(file_name.data(), "r");
    if (fp == nullptr) {
        return (uint32_t)0;
    }
    char buf[8] = { 0 };
    fgets(buf, sizeof(buf), fp);
    fclose(fp);
    return (uint32_t)atoi(buf);
}

inline auto get_veth_peer_card_iflink() {
    std::unordered_map<std::string, uint32_t> iflinks;
    struct nl_req {
        struct nlmsghdr hdr;
        struct rtgenmsg gen;
    };

    struct sockaddr_nl kernel {};
    kernel.nl_family = AF_NETLINK;

    struct sockaddr_nl local {};
    local.nl_family = AF_NETLINK;
    local.nl_pid = getpid();
    local.nl_groups = 0;

    nl_req req{};
    req.hdr.nlmsg_len = NLMSG_LENGTH(sizeof(struct rtgenmsg));
    req.hdr.nlmsg_type = RTM_GETLINK;
    req.hdr.nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
    req.hdr.nlmsg_pid = getpid();
    req.hdr.nlmsg_seq = 1;
    req.gen.rtgen_family = AF_PACKET;

    struct iovec iov {};
    iov.iov_base = &req;
    iov.iov_len = req.hdr.nlmsg_len;

    struct msghdr msg {};
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_name = &kernel;
    msg.msg_namelen = sizeof(kernel);

    auto fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
    if (fd < 0) {
        return iflinks;
    }
    auto closer = std::shared_ptr<void>(nullptr, [fd](auto) { close(fd); });

    auto ok = bind(fd, (struct sockaddr*)&local, sizeof(local));
    if (ok < 0) {
        return iflinks;
    }
    sendmsg(fd, (struct msghdr*)&msg, 0);

    memset(&iov, 0, sizeof(iov));
    constexpr int buf_size = 1024 * 32;
    char buf[buf_size]{};
    iov.iov_base = buf;
    iov.iov_len = buf_size;
    int64_t msg_len = 0;

    while (true) {
        msg_len = recvmsg(fd, &msg, 0);
        if (msg_len < 0) {
            break;
        }
        auto nlmsg_ptr = (struct nlmsghdr*)buf;
        if (nlmsg_ptr->nlmsg_type == NLMSG_DONE || nlmsg_ptr->nlmsg_type == NLMSG_ERROR) {
            break;
        }

        while (NLMSG_OK(nlmsg_ptr, msg_len)) {
            if (nlmsg_ptr->nlmsg_type == RTM_NEWLINK) {
                auto ifi_ptr = (struct ifinfomsg*)NLMSG_DATA(nlmsg_ptr);
                auto attr_ptr = IFLA_RTA(ifi_ptr);
                auto attr_len = nlmsg_ptr->nlmsg_len - NLMSG_LENGTH(sizeof(*ifi_ptr));
               
                std::string ifname;
                uint32_t iflink = 0;
                while (RTA_OK(attr_ptr, attr_len)) {             
                    if (attr_ptr->rta_type == IFLA_IFNAME) {
                        ifname = (char*)RTA_DATA(attr_ptr);          
                    }
                    if (attr_ptr->rta_type == IFLA_LINK) {
                        iflink = *((uint32_t*)RTA_DATA(attr_ptr));
                    }
                    attr_ptr = RTA_NEXT(attr_ptr, attr_len);                   
                }
                if (iflink != 0) {
                    iflinks.emplace(std::move(ifname), iflink);
                }
            }
            nlmsg_ptr = NLMSG_NEXT(nlmsg_ptr, msg_len);
        }
    }
    return iflinks;
}

using ipv46_set = std::pair<std::unordered_set<std::string>, std::unordered_set<std::string>>;
using container_ip_type = std::unordered_map<uint32_t, ipv46_set>;
inline void get_container_ip_impl(std::error_code& ec, container_ip_type& set) {
    ec.clear();
    ifaddrs* ifList{};
    if (getifaddrs(&ifList) < 0) {
        ec = std::error_code(errno, std::system_category());
        return;
    }
    auto iflinks = get_veth_peer_card_iflink();

    for (auto ifa = ifList; ifa != nullptr; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == nullptr) {
            continue;
        }
        std::string name = ifa->ifa_name;
        if (name == "lo" || name == "tunl0") {
            continue;
        }
        auto it = iflinks.find(name);
        if (it == iflinks.end()) {
            continue;
        }

        auto& [ipv4, ipv6] = set[it->second];
        if (ifa->ifa_addr->sa_family == AF_INET) {
            char address_ip[INET_ADDRSTRLEN]{};
            auto sin = (struct sockaddr_in*)ifa->ifa_addr;
            inet_ntop(AF_INET, &sin->sin_addr, address_ip, INET_ADDRSTRLEN);
            if (strncmp("169.254", address_ip, 7) != 0) { //filter Link-local address
                ipv4.emplace(address_ip);
            }
        }
        else if (ifa->ifa_addr->sa_family == AF_INET6) {
            char address_ip[INET6_ADDRSTRLEN]{};
            auto sin6 = (struct sockaddr_in6*)ifa->ifa_addr;
            inet_ntop(AF_INET6, &sin6->sin6_addr, address_ip, INET6_ADDRSTRLEN);
            if (strncmp("fe80", address_ip, 4) != 0) { //filter Link-local address
                ipv6.emplace(address_ip);
            }
        }
    }
    freeifaddrs(ifList);
}
inline auto get_container_ip(std::error_code& ec) {
    ec.clear();

    //enter a new net namespace may change some thing, so a thread is required.
    auto result = std::async(std::launch::async, [&ec]() {
        container_ip_type ip_set;
        namespace fs = std::filesystem;
        try {
            //get net namespace different from process 1.
            auto process_1_net_ns_flag = fs::read_symlink("/proc/1/ns/net").string();
            std::set<std::string> new_net_ns;
            for (const auto& entry : fs::directory_iterator("/proc/")) {
                auto net = entry.path().string() + "/ns/net";
                if (!fs::exists(net) || fs::read_symlink(net).string() == process_1_net_ns_flag) {
                    continue;
                }

                //enter each net namespace to get ip
                auto fd = open(net.data(), O_RDONLY);
                if (fd == -1) {
                    ec = std::error_code(errno, std::system_category());
                    continue;
                }
                auto closer = std::shared_ptr<void>(nullptr, [fd](auto) { close(fd); });
                if (setns(fd, 0) == -1) {
                    ec = std::error_code(errno, std::system_category());
                    continue;
                }
                get_container_ip_impl(ec, ip_set);
            }
        }
        catch (const fs::filesystem_error& e) {
            ec = e.code();
        }
        return ip_set;
    });
    return result.get();
}

inline network_card_t get_network_card(std::error_code& ec) {
    ec.clear();
    ifaddrs* ifList{};
    if (getifaddrs(&ifList) < 0) {
        ec = std::error_code(errno, std::system_category());
        return {};
    }
    auto virtual_cards = get_virtual_network_card();

    bool has_veth_pair_card = false;
    network_card_t cards;
    for (auto ifa = ifList; ifa != nullptr; ifa = ifa->ifa_next) {
        std::string name = ifa->ifa_name;
        auto it = cards.find(name);
        if (it == cards.end()) {
            networkcard card{};
            auto iflink = get_network_card_iflink(name);
            auto ifindex = if_nametoindex(name.data());
            has_veth_pair_card = (iflink != ifindex);
            card.ifindex = ifindex;
            card.is_down = get_network_card_state(name, ec) == card_state::down ? true : false;
            card.is_physics = (virtual_cards.find(name) == virtual_cards.end());
            card.real_name = name;
            card.friend_name = name;
            card.desc = name;
            card.receive_speed = get_network_card_speed(name, ec);
            card.transmit_speed = card.receive_speed;
            cards.emplace(name, std::move(card));
            it = cards.find(name);
        }

        if (ifa->ifa_addr == nullptr) {
            continue;
        }
        if (ifa->ifa_addr->sa_family == AF_INET) {
            char address_ip[INET_ADDRSTRLEN]{};
            auto sin = (struct sockaddr_in*)ifa->ifa_addr;
            inet_ntop(AF_INET, &sin->sin_addr, address_ip, INET_ADDRSTRLEN);
            if (strncmp("169.254", address_ip, 7) != 0) { //filter Link-local address
                it->second.ipv4.emplace(address_ip);
            }
        }
        else if (ifa->ifa_addr->sa_family == AF_INET6) {
            char address_ip[INET6_ADDRSTRLEN]{};
            auto sin6 = (struct sockaddr_in6*)ifa->ifa_addr;
            inet_ntop(AF_INET6, &sin6->sin6_addr, address_ip, INET6_ADDRSTRLEN);
            if (strncmp("fe80", address_ip, 4) != 0) { //filter Link-local address
                it->second.ipv6.emplace(address_ip);
            }
        }
    }
    freeifaddrs(ifList);

    if (!has_veth_pair_card) {
        return cards;
    }
    std::error_code ig;
    auto ips = get_container_ip(ec);
    for (auto& [name, info] : cards) {
        if (auto it = ips.find(info.ifindex);
            it != ips.end() && info.ipv4.empty() && info.ipv6.empty()) { //this is a container card
            auto&& [ipv4, ipv6] = it->second;
            info.ipv4 = std::move(std::move(ipv4));
            info.ipv6 = std::move(std::move(ipv6));
        }
    }
    return cards;
}

template<typename C>
inline card_flow get_network_card_flow(C&& c, std::error_code& ec)
{
    ec.clear();
    FILE* fp = fopen("/proc/net/dev", "r");
    if (fp == nullptr) {
        ec = std::error_code(errno, std::system_category());
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

        sscanf(buf, "%[^:]: %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu",
            interface,
            &receive_bytes, &receive_packets, &receive_errs, &receive_drop,
            &receive_fifo, &receive_frame, &receive_compressed, &receive_multicast,
            &transmit_bytes, &transmit_packets, &transmit_errs, &transmit_drop,
            &transmit_fifo, &transmit_colls, &transmit_carrier, &transmit_compressed);

        size_t i = 0;
        while (interface[i] == ' ') {
            i++;
            continue;
        }
        std::string name{ interface + i, strlen(interface + i) };

        auto it = c.find(name);
        if (it != c.end()) {
            flow.emplace(std::move(name), std::pair{ receive_bytes, transmit_bytes });
        }
        memset(buf, 0, sizeof(buf));
        memset(interface, 0, sizeof(interface));
    }
    fclose(fp);
    return flow;
}

inline disk_info get_disk_info(std::string_view name, std::error_code& ec) {
    ec.clear();
    struct statfs disk_Info {};
    auto ret = statfs(name.data(), &disk_Info);
    if (ret != 0) {
        ec = std::error_code(errno, std::system_category());
        return {};
    }

    disk_info info{};
    uint64_t block_size = disk_Info.f_bsize;
    info.total_size = block_size * disk_Info.f_blocks; //byte
    info.available_size = disk_Info.f_bavail * block_size;//byte
    return info;
}

inline auto tcp_used_port(std::error_code& ec) {
    ec.clear();
    std::unordered_set<uint16_t> tcp_ports;

    auto parse_port = [](std::string_view buf_view, std::unordered_set<uint16_t>& ports) {
        auto pos = buf_view.find(':');
        if (pos == std::string_view::npos) {
            return;
        }
        buf_view = buf_view.substr(pos + 1);
        pos = buf_view.find(':');
        if (pos == std::string_view::npos) {
            return;
        }

        auto port_str = std::string(buf_view.substr(pos + 1, 4));
        char* end_ptr = nullptr;
        auto port = static_cast<uint16_t>(std::strtoull(port_str.data(), &end_ptr, 16));
        if (end_ptr != port_str.data()) {
            ports.emplace(port);
        }
    };
    
    FILE* fp = fopen("/proc/net/tcp", "r");
    if (fp == nullptr) {
        ec = std::error_code(errno, std::system_category());
        return tcp_ports;
    }
    char buf[1024]{};
    while (fgets(buf, sizeof(buf), fp)) {
        std::string_view buf_view = buf;
        parse_port(buf_view, tcp_ports);
    }
    fclose(fp);

    auto fp6 = fopen("/proc/net/tcp6", "r");
    if (fp6 == nullptr) {
        ec = std::error_code(errno, std::system_category());
        return tcp_ports;
    }
    while (fgets(buf, sizeof(buf), fp6)) {
        std::string_view buf_view = buf;
        parse_port(buf_view, tcp_ports);
    }
    fclose(fp6);
    return tcp_ports;
}

inline auto get_environment_variable(std::string_view key, std::error_code& ec) {
    ec.clear();

    char* path = getenv(key.data());
    if (path == nullptr) {
        ec = std::error_code(errno, std::system_category());
        return std::string{};
    }
    return std::string{ path };
}

}
}
