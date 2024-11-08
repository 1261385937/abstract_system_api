#pragma once
#include <string>

#if _WIN32
#include "platform/windows/program.hpp"
namespace asa { namespace api = windows; }
#elif _AIX
#include "platform/aix/program.hpp"
namespace asa { namespace api = aix; }
#else
#include "platform/posix/program.hpp"
namespace asa { namespace api = posix; }
#endif


namespace asa {

class program {
public:
    using self_cpu_occupy = api::self_cpu_occupy;
    using identifier = api::identifier;
public:
    auto get_executable_path() {
        return api::get_executable_path();
    }

    auto get_executable_parent_path() {
        return api::get_executable_path<1>();
    }

    template<size_t UpDepth>
    auto get_executable_updepth_path() {
        return api::get_executable_path<UpDepth>();
    }

    auto get_executable_name() { 
        return api::get_executable_name(); 
    }

    auto get_cpu_occupy(std::error_code& ec) {
        return api::get_self_cpu_occupy(ec);
    }

    auto calculate_cpu_usage(const self_cpu_occupy& pre, const self_cpu_occupy& now) {
        return api::calculate_self_cpu_usage(pre, now);
    }

    auto memory_usage(std::error_code& ec) {
        return api::get_self_memory_usage(ec);
    }

    auto is_in_container() {
        return api::is_in_container();
    }

    uint32_t get_pid() {
        return api::get_self_pid();
    }

    //usage for whole cores
    void set_cgroup_cpu_limit(std::error_code& ec, float percentage) {
        api::set_cgroup_cpu_limit(ec, percentage);
    }

    void set_cgroup_memory_limit(std::error_code& ec, uint64_t limit_bytes) {
        api::set_cgroup_memory_limit(ec, limit_bytes);
    }

    void set_thread_name(const std::string& name) {
        api::set_thread_name(name);
    }

    identifier lock_file(std::error_code& ec, const std::string& file_path) {
        return api::lock_file(ec, file_path);
    }

    void unlock_file(std::error_code& ec, identifier ident) {
        api::unlock_file(ec, ident);
    }
};

}
