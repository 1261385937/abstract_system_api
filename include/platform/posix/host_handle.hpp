#pragma once
#pragma once
#include <string>
#include <unordered_map>
#include <set>
#include <stdio.h>
#include <memory>

namespace asa {
namespace posix {

struct host_handle {
	std::unique_ptr<FILE> os_fd = nullptr;
};

struct cpu_occupy {
	char name[256];
	unsigned int user;
	unsigned int nice;
	unsigned int system;
	unsigned int idle;
};

struct memory_info {
	uint64_t total;
	uint64_t available;
};

struct networkcard {
	bool is_down;
	std::string real_name;
	std::string friend_name;
	std::string desc;
	uint64_t recive_speed;
	uint64_t transmit_speed;  //Mbps
	std::set<std::string> ipv4;
	std::set<std::string> ipv6;
};
//key is card name
using network_card_t = std::unordered_map<std::string, networkcard>;

enum class card_state {
	unknown,
	up,
	down
};

//key is card name, value.first is recive bytes, value.second is transmit bytes.
using card_flow = std::unordered_map<std::string, std::pair<uint64_t, uint64_t>>;
using card_name = std::set<std::string>;

}
}