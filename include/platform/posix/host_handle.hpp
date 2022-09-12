#pragma once
#pragma once
#include <string>
#include <unordered_map>
#include <set>
#include <stdio.h>
#include <memory>

namespace asa {
namespace posix {

struct cpu_occupy {
	char name[256];
    uint64_t user;
    uint64_t nice;
    uint64_t system;
    uint64_t idle;
};

struct memory_info {
	uint64_t total;
	uint64_t free;
    uint64_t buffers;
    uint64_t cached;
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
