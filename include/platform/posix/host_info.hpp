#pragma once
#include <string>
#include <cstdint>
#include <codecvt>
#include <vector>
#include <set>
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

inline std::string get_os_info_internal(const std::string& from, const std::string& keyword)
{
	std::string os_info = "unknown system";
	std::string cmd = "cat " + from;

	FILE* fp = popen(cmd.data(), "r");
	if (fp == NULL) {
		return os_info;
	}
	char buf[256] = { 0 };
	while (fgets(buf, sizeof(buf), fp)) {
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
	pclose(fp);
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
	char buff[256] = { 0 };
	auto fd = popen("cat /proc/stat | grep -w cpu", "r");
	if (!fd) {
		return false;
	}
	fgets(buff, sizeof(buff), fd);
	sscanf(buff, "%s %u %u %u %u", occupy.name, &occupy.user, &occupy.nice, &occupy.system, &occupy.idle);
	pclose(fd);
	return true;
}

inline int32_t calculate_cpu_usage(const cpu_occupy& pre, const cpu_occupy& now)
{
	uint64_t od = pre.user + pre.nice + pre.system + pre.idle;
	uint64_t nd = now.user + now.nice + now.system + now.idle;
	auto id = now.user - pre.user;
	auto sd = now.system - pre.system;

	int usage = (int32_t)ceil((double)(sd + id) * 100.0 / (double)(nd - od));
	return usage;
}

inline int32_t get_memory_usage() {
	memory_info meminfo{};
	FILE* fp = popen("cat /proc/meminfo | grep -wE 'MemTotal:|MemAvailable:'", "r");
	if (fp == NULL) {
		return -1;
	}
	char name[256] = { 0 };
	char buf[256] = { 0 };
	char unit[32] = { 0 };
	while (fgets(buf, sizeof(buf), fp) != NULL) {
		uint64_t data;
		sscanf(buf, "%s %llu %s", name, &data, unit);
		if (!strcmp("MemTotal:", name)) {
			meminfo.total = data;
		}
		else if (!strcmp("MemAvailable:", name)) {
			meminfo.available = data;
		}
		memset(name, 0, sizeof(name));
		memset(buf, 0, sizeof(buf));
		memset(unit, 0, sizeof(unit));
	}
	pclose(fp);

	int mem_usage = (int)ceil((double)(meminfo.total - meminfo.available) * 100.0 / (double)meminfo.total);
	return mem_usage;
}


inline card_state get_network_card_state(const std::string& card_name) {
	auto cmd = "cat /sys/class/net/" + card_name + "/operstate";
	FILE* fp = popen(cmd.data(), "r");
	if (fp == nullptr) {
		return card_state::unknown;
	}
	char buf[64] = { 0 };
	fgets(buf, sizeof(buf), fp);
	pclose(fp);

	if (strncasecmp(buf, "up", 2) == 0) {
		return card_state::up;
	}
	if (strncasecmp(buf, "down", 4) == 0) {
		return card_state::down;
	}
	return card_state::unknown;
};

inline uint32_t get_network_card_speed(const std::string& card_name) {
	auto cmd = "cat /sys/class/net/" + card_name + "/speed";
	FILE* fp = popen(cmd.data(), "r");
	if (fp == nullptr) {
		return 0;
	}
	char buf[64] = { 0 };
	fgets(buf, sizeof(buf), fp);
	pclose(fp);
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

inline card_flow get_network_card_flow(const card_name& names) 
{
	std::string cmd = "cat /proc/net/dev | grep -E ";
	std::string comb = "'";
	for (const auto& name : names) {
		comb.append(name).append("|");
	}
	comb.erase(comb.length() - 1);
	comb.append("'");
	cmd.append(comb);

	FILE* fp = popen(cmd.data(), "r");
	if (fp == nullptr) {
		return {};
	}
	char buf[1024] = { 0 };
	char interface[256]{};
	card_flow flow;
	while (fgets(buf, sizeof(buf), fp) != nullptr) {
		uint64_t receive_bytes = 0;
		uint64_t receive_packets;
		uint64_t receive_errs;
		uint64_t receive_drop;
		uint64_t receive_fifo;
		uint64_t receive_frame;
		uint64_t receive_compressed;
		uint64_t receive_multicast;

		uint64_t transmit_bytes = 0;
		uint64_t transmit_packets;
		uint64_t transmit_errs;
		uint64_t transmit_drop;
		uint64_t transmit_fifo;
		uint64_t transmit_colls;
		uint64_t transmit_carrier;
		uint64_t transmit_compressed;

		sscanf(buf, "%s %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu",
			interface,
			&receive_bytes, &receive_packets, &receive_errs, &receive_drop,
			&receive_fifo, &receive_frame, &receive_compressed, &receive_multicast,
			&transmit_bytes, &transmit_packets, &transmit_errs, &transmit_drop,
			&transmit_fifo, &transmit_colls, &transmit_carrier, &transmit_compressed);

		std::string name{ interface, strlen(interface) - 1 };
		auto it = names.find(name);
		if (it != names.end()) {
			flow.emplace(std::move(name), std::pair{ receive_bytes ,transmit_bytes });
		}
		memset(buf, 0, sizeof(buf));
		memset(interface, 0, sizeof(interface));
	}
	pclose(fp);
	return flow;
}


}
}