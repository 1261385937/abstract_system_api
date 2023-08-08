#pragma once
#pragma once
#include <string>
#include <unordered_map>
#include <unordered_set>
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
	uint32_t iflink;
	bool is_down;
	bool is_physics;
	std::string real_name;
	std::string friend_name;
	std::string desc;
	uint64_t receive_speed;
	uint64_t transmit_speed;  //Mbps
	std::unordered_set<std::string> ipv4;
	std::unordered_set<std::string> ipv6;
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
using card_name = std::unordered_set<std::string>;

struct disk_info {
    uint64_t total_size;
    uint64_t available_size;
};

}
}
