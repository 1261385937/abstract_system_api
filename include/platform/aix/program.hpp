#pragma once
#include <sys/sysinfo.h>
#include <string>
#include <system_error>
#include <regex>
#include <filesystem>
#include <unistd.h>
#include <limits.h>

namespace asa {
namespace aix {

constexpr char executable_name[] = "new_pcap_agent";

template<size_t UpDepth = 0>
inline std::string get_executable_path() {
    thread_local std::string exe_path;
    if (!exe_path.empty()) {
        return exe_path;
    }

    constexpr char templ[] = R"(ps -ef | grep %s | grep -v grep | awk '{print $8}')";
    char cmd[128]{};
    sprintf(cmd, templ, executable_name);

    auto fd = popen(cmd, "r");
    if (fd == nullptr) {
        return {};
    }
    char buf[1024] = { 0 };
    fgets(buf, sizeof(buf), fd);
    pclose(fd);

    std::string_view path;
    std::string tmp_path;
    if (strncmp(buf, "./", 2) != 0) { //start with full path
        path = buf;
        path = path.substr(0, path.find_last_of(R"(/)") + 1);
    }
    else { //Get full path with getcwd when start with ./
        memset(buf, 0, sizeof(buf));
        if (getcwd(buf, sizeof(buf)) == nullptr) {
            return {};
        }
        tmp_path = std::string(buf) + "/";
        path = tmp_path;
    }

    printf("path_view is %s\n", std::string(path).data());

    if constexpr (UpDepth == 0) {
        exe_path = std::string{ path.data(),path.length() };
    }
    else {
        for (size_t i = 0; i < UpDepth; i++) {
            path = path.substr(0, path.length() - 1);
            path = path.substr(0, path.find_last_of(R"(/)") + 1);
        }
        exe_path = std::string{ path.data(),path.length() };
    }
    return exe_path; 
}

inline std::string get_executable_name() {
	return executable_name;
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
    return std::make_tuple(0.1, (uint64_t)(100 * 1024 * 1024), (uint64_t)8 * 1024 * 1024 * 1024);
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
