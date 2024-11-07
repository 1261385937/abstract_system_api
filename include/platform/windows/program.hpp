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
    }
    else {
        for (size_t i = 0; i < UpDepth; i++) {
            path = path.substr(0, path.length() - 1);
            path = path.substr(0, path.find_last_of(R"(\)") + 1);
        }
        return std::string{path.data(), path.length()};
    }
}

inline std::string get_executable_name() {
    // the lengh is enough
    constexpr size_t len = 2048;
    char full_path[len]{};
    std::string_view path;
    GetModuleFileNameA(nullptr, full_path, len);
    path = full_path;
    std::string name{path.substr(path.find_last_of(R"(\)") + 1)};
    return name;
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

inline auto get_self_memory_usage(std::error_code& ec) {
    ec.clear();
    MEMORYSTATUSEX ex{};
    ex.dwLength = sizeof(ex);
    auto ok = GlobalMemoryStatusEx(&ex);
    if (!ok) {
        ec = std::error_code(GetLastError(), std::system_category());
        return std::make_tuple(0.0, size_t(0), size_t(0));
    }

    PROCESS_MEMORY_COUNTERS pmc{};
    ok = GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc));
    if (!ok) {
        ec = std::error_code(GetLastError(), std::system_category());
        return std::make_tuple(0.0, size_t(0), size_t(0));
    }
    auto usage = pmc.WorkingSetSize * 100.0 / ex.ullTotalPhys;
    return std::make_tuple(usage, pmc.WorkingSetSize, ex.ullTotalPhys);
}

inline bool is_in_container() { 
    return false; 
}

inline uint32_t get_self_pid() {
    return GetCurrentProcessId();
}

inline void set_cgroup_cpu_limit(std::error_code& ec, float percentage) {
    ec.clear();

    auto job = CreateJobObjectA(nullptr, nullptr);
    if (job == nullptr) {
        ec = std::error_code(GetLastError(), std::system_category());
        return;
    }
    auto closer = std::shared_ptr<void>(nullptr, [job](auto) {CloseHandle(job); });

    JOBOBJECT_CPU_RATE_CONTROL_INFORMATION cpu_limit{};
    cpu_limit.ControlFlags = 
        JOB_OBJECT_CPU_RATE_CONTROL_ENABLE | JOB_OBJECT_CPU_RATE_CONTROL_HARD_CAP;
    cpu_limit.CpuRate = static_cast<DWORD>(percentage * 100);
    auto ret = SetInformationJobObject(job,
        JobObjectCpuRateControlInformation, &cpu_limit, sizeof(cpu_limit));
    if (ret == 0) {
        ec = std::error_code(GetLastError(), std::system_category());
        return;
    }
    ret = AssignProcessToJobObject(job, GetCurrentProcess());
    if (ret == 0) {
        ec = std::error_code(GetLastError(), std::system_category());
    }
}

inline void set_cgroup_memory_limit(std::error_code& ec, uint64_t limit_bytes) {
    ec.clear();

    auto job = CreateJobObjectA(nullptr, nullptr);
    if (job == nullptr) {
        ec = std::error_code(GetLastError(), std::system_category());
        return;
    }
    auto closer = std::shared_ptr<void>(nullptr, [job](auto) {CloseHandle(job); });

    JOBOBJECT_EXTENDED_LIMIT_INFORMATION mem_limit{};
    mem_limit.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_JOB_MEMORY;
    mem_limit.JobMemoryLimit = limit_bytes;
    auto ret = SetInformationJobObject(job,
        JobObjectExtendedLimitInformation, &mem_limit, sizeof(mem_limit));
    if (ret == 0) {
        ec = std::error_code(GetLastError(), std::system_category());
        return;
    }
    ret = AssignProcessToJobObject(job, GetCurrentProcess());
    if (ret == 0) {
        ec = std::error_code(GetLastError(), std::system_category());
    }
}

inline void set_thread_name(const std::string& name) {
    const DWORD ms_vc_exception = 0x406D1388;
#pragma pack(push,8)
    struct thread_name_info {
        DWORD type = 0x1000; // Must be 0x1000.  
        LPCSTR name; // Pointer to name (in user addr space).  
        DWORD thread_id = GetCurrentThreadId(); // Thread ID (-1=caller thread).  
        DWORD flags = 0; // Reserved for future use, must be zero.  
    };
#pragma pack(pop)

    thread_name_info info{};
    info.name = name.data();  
#pragma warning(push)
#pragma warning(disable: 6320 6322)
    __try {
        RaiseException(ms_vc_exception, 0, sizeof(info) / sizeof(ULONG_PTR), (ULONG_PTR*)&info);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
#pragma warning(pop)  
}

using identifier = HANDLE;
inline identifier lock_file(std::error_code& ec, const std::string& file_path) {
    ec.clear();

    auto handle = CreateFileA(file_path.c_str(), GENERIC_READ | GENERIC_WRITE,
        0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        ec = std::error_code(GetLastError(), std::system_category());
        return INVALID_HANDLE_VALUE;
    }

    OVERLAPPED overlapped = { 0 };
    if (!LockFileEx(handle, LOCKFILE_EXCLUSIVE_LOCK, 0, 0xFFFFFFFF, 0xFFFFFFFF, &overlapped)) {
        ec = std::error_code(GetLastError(), std::system_category());
        CloseHandle(handle);
        return INVALID_HANDLE_VALUE;
    }
    return handle;
}

inline void unlock_file(std::error_code& ec, identifier ident) {
    ec.clear();

    OVERLAPPED overlapped = { 0 };
    if (!UnlockFileEx(ident, 0, 0xFFFFFFFF, 0xFFFFFFFF, &overlapped)) {
        ec = std::error_code(GetLastError(), std::system_category());
    }
    CloseHandle(ident);
}



}  // namespace windows
}  // namespace asa
