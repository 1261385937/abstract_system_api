#pragma once
#include <thread>

#if _WIN32
#include "platform/windows/host_info.hpp"

namespace asa { namespace api = windows; }
#else
#include "platform/posix/host_info.hpp"

namespace asa { namespace api = posix; }
#endif

namespace asa {

class host {
public:
	using network_card_t = api::network_card_t;
	using card_flow = api::card_flow;
	using card_name = api::card_name;
	using cpu_occupy = api::cpu_occupy;

public:
	std::string os_info() {
		return api::get_os_info();
	}

	std::string hostname() {
		return api::get_hostname();
	}

	//this function sleep 1s inside
	//return -1 if error
	int32_t cpu_usage_1s() {
		cpu_occupy pre{};
		if (!api::get_cpu_occupy(pre)) {
			return -1;
		}
		using namespace std::chrono_literals;
		std::this_thread::sleep_for(1000ms);
		cpu_occupy now{};
		if (!api::get_cpu_occupy(now)) {
			return -1;
		}
		return api::calculate_cpu_usage(pre, now);
	}

	auto get_cpu_occupy(cpu_occupy& occupy) {
		return api::get_cpu_occupy(occupy);
	}

	auto calculate_cpu_usage(const cpu_occupy& pre, const cpu_occupy& now) {
		return api::calculate_cpu_usage(pre, now);
	}

	//return -1 if error
	int32_t memory_usage() {
		return api::get_memory_usage();
	}

	network_card_t network_card() {
		return api::get_network_card();
	}

	auto calculate_network_card_flow(uint32_t interval_s,
		const card_flow& pre_flow, const card_flow& now_flow)
	{
		card_flow flow;
		for (const auto& [name, pair] : pre_flow) {
			auto it = now_flow.find(name);
			if (it == now_flow.end()) {
				continue; //network_card not match
			}
			auto& now = it->second;
			auto& pre = pair;
			auto recive = (now.first - pre.first) / interval_s; //recive
			auto transmit = (now.second - pre.second) / interval_s; //transmit
			flow.emplace(name, std::pair{ recive, transmit });
		}
		return flow;
	}

	//recommend use this function, sleep 1s inside
	card_flow network_card_flow_1s(const card_name& names) {
		auto pre = api::get_network_card_flow(names);
		using namespace std::chrono_literals;
		std::this_thread::sleep_for(1000ms);
		auto now = api::get_network_card_flow(names);
		return calculate_network_card_flow(1, pre, now);
	}

	auto get_network_card_flow(const card_name& names) {
		return api::get_network_card_flow(names);
	}
	
};

}
