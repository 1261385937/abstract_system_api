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
	std::string get_executable_path() {
		return api::get_executable_path();
	}

	std::string get_executable_parent_path() {
		return api::get_executable_path<1>();
	}
};

}