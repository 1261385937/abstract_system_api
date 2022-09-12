#pragma once
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace asa {
namespace windows {

struct cpu_occupy {
	uint64_t idle_time;
	uint64_t kernel_time;
	uint64_t user_time;
};

struct networkcard {
	bool is_down;
	std::string real_name;
	std::string friend_name;
	std::string desc; //"Software Loopback Interface 1"
	uint64_t recive_speed;
	uint64_t transmit_speed; //Mbps
	std::unordered_set<std::string> ipv4;
	std::unordered_set<std::string> ipv6;
};
//key is card name
using network_card_t = std::unordered_map<std::string, networkcard>;

//key is card name, value.first is recive bytes, value.second is transmit bytes.
using card_flow = std::unordered_map<std::string, std::pair<uint64_t, uint64_t>>;
using card_name = std::unordered_set<std::string>;


}
}
