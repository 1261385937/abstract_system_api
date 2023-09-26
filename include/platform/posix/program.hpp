#pragma once
#include <unistd.h>
#include <sys/sysinfo.h>
#include <string>
#include <system_error>
#include <regex>
#include <filesystem>
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

inline std::string get_executable_name() {
    // the lengh is enough
    constexpr size_t len = 2048;
    char full_path[len]{};
    std::string_view path;
    readlink("/proc/self/exe", full_path, len);
    path = full_path;
    std::string name{path.substr(path.find_last_of(R"(/)") + 1)};
    return name;
}

struct self_cpu_occupy {
    uint64_t user_time;
    uint64_t sys_time;
    uint64_t total_time;
};

inline self_cpu_occupy get_self_cpu_occupy(std::error_code& ec) {
    ec.clear();
    FILE* fp = fopen("/proc/self/stat", "r"); //14 15
    if (fp == nullptr) {
        ec = std::error_code(errno, std::system_category());
        return {};
    }
    self_cpu_occupy occupy{};
    char buf[512] = { 0 };
    fgets(buf, sizeof(buf), fp);
    sscanf(buf, "%*s %*s %*s %*s %*s %*s %*s %*s %*s %*s %*s %*s %*s %llu %llu",
        &occupy.user_time, &occupy.sys_time);
    fclose(fp);

    fp = fopen("/proc/stat", "r");
    if (fp == nullptr) {
        ec = std::error_code(errno, std::system_category());
        return {};
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
    return occupy;
}

inline double calculate_self_cpu_usage(const self_cpu_occupy& pre, const self_cpu_occupy& now)
{
    auto pre_used = pre.user_time + pre.sys_time;
    auto now_used = now.user_time + now.sys_time;
    auto used_detal = now_used - pre_used;
    auto total_detal = now.total_time - pre.total_time;
    if (total_detal == 0) {
        return {};
    }

    auto self_cpu_usage = (double)used_detal * 100 / (double)total_detal;
    return self_cpu_usage;
}

inline auto get_self_memory_usage(std::error_code& ec) {
    ec.clear();
    FILE* fp = fopen("/proc/self/status", "r");
    if (fp == nullptr) {
        ec = std::error_code(errno, std::system_category());
        return std::make_tuple(0.0, size_t(0), size_t(0));
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
        ec = std::error_code(errno, std::system_category());
        return std::make_tuple(0.0, size_t(0), size_t(0));
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

    if (mem_total == 0) {
        return std::make_tuple(0.0, size_t(0), size_t(0));;
    }
    auto self_mem_usage = (double)self_vm_rss * 100 / (double)mem_total;
    return std::make_tuple(self_mem_usage, self_vm_rss, mem_total);
}

inline bool is_in_container() {
    FILE* fp = fopen("/proc/self/cgroup", "r");
    if (fp == nullptr) {
        return false;
    }
    auto regex_id = std::regex(R"(^.*/(?:.*-)?([0-9a-f]+)(?:\.|\s*$))");
    char buf[1024] = { 0 };

    while (fgets(buf, 1024, fp)) {
        if ((strstr(buf, "docker") != nullptr) ||
            (strstr(buf, "kubepods") != nullptr) ||
            (strstr(buf, "lxc") != nullptr) ||
            (strstr(buf, "rkt") != nullptr) ||
            (strstr(buf, "sandbox") != nullptr)) {
            return true;
        }
        // maybe useful for unknown container
        if (std::regex_match(buf, regex_id)) {
            return true;
        }
        memset(buf, 0, sizeof(buf));
    }
    fclose(fp);
    return false;
}

inline int get_self_pid() {
    return getpid();
}

inline void set_cgroup_cpu_limit(std::error_code& ec, float percentage) {
    ec.clear();
    auto set_cgroup_cpu = [&ec, percentage](const std::string& cpu_path) {
        auto self_cpu_path = cpu_path + get_executable_name();
        std::filesystem::create_directory(self_cpu_path, ec);

        //period
        int defualt_cfs_period_us = 100000;
        {
            auto cpu_period_path = self_cpu_path + "/cpu.cfs_period_us";
            auto fp = fopen(cpu_period_path.data(), "r");
            if (fp == nullptr) {
                ec = std::error_code(errno, std::system_category());
                return;
            }
            char buf[128] = { 0 };
            fgets(buf, sizeof(buf), fp);
            sscanf(buf, "%d", &defualt_cfs_period_us);
            fclose(fp);
        }
        //quota
        {
            int cfs_quota_us = static_cast<int>(
                static_cast<float>(defualt_cfs_period_us * get_nprocs()) * percentage) / 100;
            auto cpu_quota_path = self_cpu_path + "/cpu.cfs_quota_us";
            auto fp = fopen(cpu_quota_path.data(), "wb");
            if (fp == nullptr) {
                ec = std::error_code(errno, std::system_category());
                return;
            }
            fprintf(fp, "%d", cfs_quota_us);
            fclose(fp);
        }
        //tasks
        {
            auto cpu_task_path = self_cpu_path + "/tasks";
            auto fp = fopen(cpu_task_path.data(), "wb");
            if (fp == nullptr) {
                ec = std::error_code(errno, std::system_category());
                return;
            }
            fprintf(fp, "%d", get_self_pid());
            fclose(fp);
        }
    };

    std::string cpu_path = "/sys/fs/cgroup/cpu/";
    auto exist = std::filesystem::exists(cpu_path, ec);
    if (exist) {
        set_cgroup_cpu(cpu_path);
        return;
    }

    std::string old_cpu_path = "/cgroup/cpu/";
    exist = std::filesystem::exists(cpu_path, ec);
    if (exist) {
        set_cgroup_cpu(cpu_path);
    }
}

inline void set_cgroup_memory_limit(std::error_code& ec, uint64_t limit_bytes) {
    ec.clear();
    auto set_cgroup_memory = [&ec, limit_bytes](const std::string& memory_path) {
        auto self_memory_path = memory_path + get_executable_name();
        std::filesystem::create_directory(self_memory_path, ec);

        //memory limit
        {
            auto mem_limit_path = self_memory_path + "/memory.limit_in_bytes";
            auto fp = fopen(mem_limit_path.data(), "wb");
            if (fp == nullptr) {
                ec = std::error_code(errno, std::system_category());
                return;
            }
            fprintf(fp, "%llu", limit_bytes);
            fclose(fp);
        }
        //memory swap limit, some linux do not support this feature
        {
            auto mem_limit_path = self_memory_path + "/memory.memsw.limit_in_bytes";
            auto fp = fopen(mem_limit_path.data(), "wb");
            if (fp != nullptr) {
                fprintf(fp, "%llu", limit_bytes);
                fclose(fp);
            }
        }
        //tasks
        {
            auto mem_task_path = self_memory_path + "/tasks";
            auto fp = fopen(mem_task_path.data(), "wb");
            if (fp == nullptr) {
                ec = std::error_code(errno, std::system_category());
                return;
            }
            fprintf(fp, "%d", get_self_pid());
            fclose(fp);
        }
    };

    std::string memory_path = "/sys/fs/cgroup/memory/";
    auto exist = std::filesystem::exists(memory_path, ec);
    if (exist) {
        set_cgroup_memory(memory_path);
        return;
    }

    std::string old_memory_path = "/cgroup/memory/";
    exist = std::filesystem::exists(old_memory_path, ec);
    if (exist) {
        set_cgroup_memory(old_memory_path);
    }
}

}
}
