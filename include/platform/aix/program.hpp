#pragma once
#include <unistd.h>
#include <sys/sysinfo.h>
#include <string>
#include <system_error>
#include <regex>
#include <filesystem>
#include <unistd.h>

namespace asa {
namespace aix {

template<size_t UpDepth = 0>
inline std::string get_executable_path() {
	std::string current_path = std::filesystem::current_path().string();
	std::string_view path = current_path;
	if constexpr (UpDepth == 0) {
		return std::string{ path.data(),path.length() };
	}
	else {
		for (size_t i = 0; i < UpDepth; i++) {
			path = path.substr(0, path.length() - 1);
			path = path.substr(0, path.find_last_of(R"(/)") + 1);
		}
		return std::string{ path.data(),path.length() };
	}
}

inline std::string get_executable_name() {
	return "new_pcap_agent";
}

struct self_cpu_occupy {
    uint64_t user_time;
    uint64_t sys_time;
    uint64_t total_time;
};

inline self_cpu_occupy get_self_cpu_occupy(std::error_code& ec) {
    ec.clear();
    return {};
}

inline double calculate_self_cpu_usage(const self_cpu_occupy&, const self_cpu_occupy&)
{
    return {};
}

inline auto get_self_memory_usage(std::error_code& ec) {
    ec.clear();
    return std::make_tuple(0.1, 100 * 1024 * 1024, 8 * 1024 * 1024 * 1024);
}

inline bool is_in_container() {
    return false;
}

inline int get_self_pid() {
    return getpid();
}

inline void set_cgroup_cpu_limit(std::error_code& ec, float) {
    ec.clear();
}

inline void set_cgroup_memory_limit(std::error_code& ec, uint64_t) {
    ec.clear();
}

}
}
