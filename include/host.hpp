#pragma once
#include <thread>

#if _WIN32
#include "platform/windows/host_info.hpp"

namespace asa {
namespace api = windows;
}
#else
#include "platform/posix/host_info.hpp"

namespace asa {
namespace api = posix;
}
#endif

namespace asa {

class host {
public:
    using network_card_t = api::network_card_t;
    using card_flow = api::card_flow;
    using card_name = api::card_name;
    using cpu_occupy = api::cpu_occupy;
    using disk_info = api::disk_info;

public:
    auto os_info(std::error_code& ec) { return api::get_os_info(ec); }

    auto hostname(std::error_code& ec) { return api::get_hostname(ec); }

    auto get_cpu_occupy(std::error_code& ec) { return api::get_cpu_occupy(ec); }

    auto calculate_cpu_usage(const cpu_occupy& pre, const cpu_occupy& now) {
        return api::calculate_cpu_usage(pre, now);
    }

    auto memory_usage(std::error_code& ec) { return api::get_memory_usage(ec); }

    auto network_card(std::error_code& ec) { return api::get_network_card(ec); }

    auto calculate_network_card_flow(uint32_t interval_s,
                                     const card_flow& pre_flow,
                                     const card_flow& now_flow) {
        card_flow flow;
        for (const auto& [name, pair] : pre_flow) {
            auto it = now_flow.find(name);
            if (it == now_flow.end()) {
                continue;  // network_card not match
            }
            auto& now = it->second;
            auto& pre = pair;
#if _WIN32
            auto recive = (static_cast<DWORD>(now.first) -
                           static_cast<DWORD>(pre.first)) /
                          interval_s;  // recive
            auto transmit = (static_cast<DWORD>(now.second) -
                             static_cast<DWORD>(pre.second)) /
                            interval_s;  // transmit
#else
            auto recive = (now.first - pre.first) / interval_s;      // recive
            auto transmit = (now.second - pre.second) / interval_s;  // transmit
#endif
            flow.emplace(name, std::pair{recive, transmit});
        }
        return flow;
    }

    auto get_network_card_flow(const card_name& names, std::error_code& ec) {
        return api::get_network_card_flow(names, ec);
    }

    auto get_network_card_flow(const network_card_t& cards,
                               std::error_code& ec) {
        return api::get_network_card_flow(cards, ec);
    }

    auto get_disk_info(std::string_view name, std::error_code& ec) {
        return api::get_disk_info(name, ec);
    }
};

}  // namespace asa
