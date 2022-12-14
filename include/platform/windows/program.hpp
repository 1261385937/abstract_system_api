#pragma once
#include <string>
#include <system_error>
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#define PSAPI_VERSION 1
#include <psapi.h>

#pragma comment(lib, "psapi.lib")

namespace asa {
namespace windows {

template <size_t UpDepth = 0>
inline std::string get_executable_path() {
    // the lengh is enough
    constexpr size_t len = 2048;
    char full_path[len]{};
    std::string_view path;
    GetModuleFileNameA(nullptr, full_path, len);
    path = full_path;
    path = path.substr(0, path.find_last_of(R"(\)") + 1);
    if constexpr (UpDepth == 0) {
        return std::string{path.data(), path.length()};
    } else {
        for (size_t i = 0; i < UpDepth; i++) {
            path = path.substr(0, path.length() - 1);
            path = path.substr(0, path.find_last_of(R"(\)") + 1);
        }
        return std::string{path.data(), path.length()};
    }
}

struct self_cpu_occupy {
    uint64_t kernel_time;
    uint64_t user_time;
    uint64_t sys_kernel_time;
    uint64_t sys_user_time;
};

inline self_cpu_occupy get_self_cpu_occupy(std::error_code& ec) {
    ec.clear();
    auto filetime_to_uint64_t = [](const FILETIME& time) {
        return (uint64_t)time.dwHighDateTime << 32 | time.dwLowDateTime;
    };

    FILETIME creation_time{};
    FILETIME exit_time{};
    FILETIME kernel_time{};
    FILETIME user_time{};
    auto ok = GetProcessTimes(GetCurrentProcess(), &creation_time, &exit_time,
                              &kernel_time, &user_time);
    if (!ok) {
        ec = std::error_code(GetLastError(), std::system_category());
        return {};
    }

    self_cpu_occupy occupy{};
    occupy.kernel_time = filetime_to_uint64_t(kernel_time);
    occupy.user_time = filetime_to_uint64_t(user_time);

    FILETIME sys_idle_time{};
    FILETIME sys_kernel_time{};
    FILETIME sys_user_time{};
    ok = GetSystemTimes(&sys_idle_time, &sys_kernel_time, &sys_user_time);
    if (!ok) {
        ec = std::error_code(GetLastError(), std::system_category());
        return {};
    }
    occupy.sys_kernel_time = filetime_to_uint64_t(sys_kernel_time);
    occupy.sys_user_time = filetime_to_uint64_t(sys_user_time);
    return occupy;
}

inline double calculate_self_cpu_usage(const self_cpu_occupy& pre,
                                       const self_cpu_occupy& now) {
    auto kernel_delta = now.kernel_time - pre.kernel_time;
    auto user_detal = now.user_time - pre.user_time;
    auto sys_kernel_delta = now.sys_kernel_time - pre.sys_kernel_time;
    auto sys_user_detal = now.sys_user_time - pre.sys_user_time;
    auto total_detal = sys_kernel_delta + sys_user_detal;
    if (total_detal == 0) {
        return {};
    }

    auto usage = (kernel_delta + user_detal) * 100.0 / total_detal;
    return usage;
}

inline double get_self_memory_usage(std::error_code& ec) {
    ec.clear();
    MEMORYSTATUSEX ex{};
    ex.dwLength = sizeof(ex);
    auto ok = GlobalMemoryStatusEx(&ex);
    if (!ok) {
        ec = std::error_code(GetLastError(), std::system_category());
        return {};
    }

    PROCESS_MEMORY_COUNTERS pmc{};
    ok = GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc));
    if (!ok) {
        ec = std::error_code(GetLastError(), std::system_category());
        return {};
    }
    auto usage = pmc.WorkingSetSize * 100.0 / ex.ullTotalPhys;
    return usage;
}

inline bool is_in_container() { return false; }

}  // namespace windows
}  // namespace asa
