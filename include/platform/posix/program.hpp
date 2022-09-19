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
    uint64_t user_time;
    uint64_t sys_time;
    uint64_t total_time;
};

inline bool get_self_cpu_occupy(self_cpu_occupy& occupy) {
    FILE* fp = fopen("/proc/self/stat", "r"); //14 15
    if (fp == nullptr) {
        return false;
    }

    char buf[512] = { 0 };
    fgets(buf, sizeof(buf), fp);
    sscanf(buf, "%*s %*s %*s %*s %*s %*s %*s %*s %*s %*s %*s %*s %*s %llu %llu",
        &occupy.user_time, &occupy.sys_time);
    fclose(fp);

    fp = fopen("/proc/stat", "r");
    if (fp == nullptr) {
        return false;
    }
    uint64_t user = 0;
    uint64_t nice = 0;
    uint64_t system = 0;
    uint64_t idle = 0;
    char name[32]{};
    while (fgets(buf, sizeof(buf), fp)) {
        sscanf(buf, "%s %llu %llu %llu %llu",
            name, &user, &nice, &system, &idle);

        if (!strcmp("cpu", name)) {
            break;
        }
        memset(buf, 0, sizeof(buf));
        memset(name, 0, sizeof(name));
    }
    fclose(fp);

    occupy.total_time = user + nice + system + idle;
    return true;
}

inline double calculate_self_cpu_usage(const self_cpu_occupy& pre, const self_cpu_occupy& now)
{
    auto pre_used = pre.user_time + pre.sys_time;
    auto now_used = now.user_time + now.sys_time;
    auto used_detal = now_used - pre_used;
    auto total_detal = now.total_time - pre.total_time;

    auto self_cpu_usage = (double)used_detal * 100 / (double)total_detal;
    return self_cpu_usage;
}

inline double get_self_memory_usage() {
    FILE* fp = fopen("/proc/self/status", "r");
    if (fp == nullptr) {
        return -1.0;
    }

    uint64_t self_vm_rss = 0;
    char name[256] = { 0 };
    char buf[256] = { 0 };
    while (fgets(buf, sizeof(buf), fp)) {   
        sscanf(buf, "%s %llu", name, &self_vm_rss);
        if (!strcmp("VmRSS:", name)) {
            break;
        }
        memset(name, 0, sizeof(name));
        memset(buf, 0, sizeof(buf));
    }
    fclose(fp);

    uint64_t mem_total = 0;
    fp = fopen("/proc/meminfo", "r");
    if (fp == nullptr) {
        return -1.0;
    }
    while (fgets(buf, sizeof(buf), fp)) {      
        sscanf(buf, "%s %llu", name, &mem_total);
        if (!strcmp("MemTotal:", name)) {
            break;
        }
        memset(name, 0, sizeof(name));
        memset(buf, 0, sizeof(buf));
    }
    fclose(fp);

    auto self_mem_usage = (double)self_vm_rss * 100 / (double)mem_total;
    return self_mem_usage;
}

}
}
