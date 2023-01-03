#pragma once
#include <string>

#if _WIN32
#include "platform/windows/program.hpp"

namespace asa { namespace api = windows; }
#else
#include "platform/posix/program.hpp"

namespace asa { namespace api = posix; }
#endif


namespace asa {

class program {
public:
    using self_cpu_occupy = api::self_cpu_occupy;
public:
    auto get_executable_path() {
        return api::get_executable_path();
    }

    auto get_executable_parent_path() {
        return api::get_executable_path<1>();
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
};

}
