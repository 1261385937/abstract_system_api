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
	std::string get_executable_path() {
		return api::get_executable_path();
	}

	std::string get_executable_parent_path() {
		return api::get_executable_path<1>();
	}

    bool get_cpu_occupy(self_cpu_occupy& occupy) {
        return api::get_self_cpu_occupy(occupy);
    }

    double calculate_cpu_usage(const self_cpu_occupy& pre, const self_cpu_occupy& now) {
        return api::calculate_self_cpu_usage(pre, now);
    }

    double memory_usage() {
        double usage = 0;
        if (!api::get_self_memory_usage(usage)) {
            return -1.0;
        }
        return usage;
    }
};

}
