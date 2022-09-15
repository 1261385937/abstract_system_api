#pragma once
#include <string>
#include <unistd.h>

namespace asa {
namespace posix {

template<size_t UpDepth = 0>
inline std::string get_executable_path()
{
	//the lengh is enough
	constexpr size_t len = 2048;
	char full_path[len]{};
	std::string_view path;
	readlink("/proc/self/exe", full_path, len);
	path = full_path;
	path = path.substr(0, path.find_last_of(R"(/)") + 1);
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

struct self_cpu_occupy {


};

inline bool get_self_cpu_occupy(self_cpu_occupy& occupy) {
    
    return true;
}

inline double calculate_self_cpu_usage(const self_cpu_occupy& pre, const self_cpu_occupy& now)
{
    
    return {};
}

inline bool get_self_memory_usage(double& usage) {
  
    return true;
}

}
}
