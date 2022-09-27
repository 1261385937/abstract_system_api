#pragma once
#include <string>
#include <tuple>
#include <system_error>
#include "process_handle.hpp"

namespace asa {
namespace windows {

constexpr inline DWORD still_active = 259;

template<typename F, std::size_t ... Index>
inline constexpr void for_each_tuple(F&& f, std::index_sequence<Index...>) {
	(std::forward<F>(f)(std::integral_constant<std::size_t, Index>()), ...);
}

template<typename ...Args>
inline child_handle start_process(std::string_view execute, Args&&... args) {
	auto tup = std::make_tuple(std::forward<Args>(args)...);
	constexpr auto tup_size = std::tuple_size_v<decltype(tup)>;

	std::string cmd_lines;
	for_each_tuple([&cmd_lines, &tup, &tup_size](auto index) {
		if constexpr (index == tup_size - 1) {
			cmd_lines.append(std::get<index>(tup));
		}
		else {
			cmd_lines.append(std::get<index>(tup)).append(" ");
		}
	}, std::make_index_sequence<tup_size>());

	STARTUPINFOA startup_info{};
	PROCESS_INFORMATION proc_info{};
	auto ok = CreateProcessA(execute.data(), cmd_lines.data(),
		nullptr, nullptr, false, 0, nullptr, nullptr, &startup_info, &proc_info);
	if (!ok) {
		throw std::system_error(
			std::error_code(GetLastError(), std::system_category()), "CreateProcess failed");
	}
	return child_handle(proc_info);
};

inline void terminate_process(child_handle& p, std::error_code& ec) {
	if (!TerminateProcess(p.process_handle(), EXIT_FAILURE)) {
		ec = std::error_code(GetLastError(), std::system_category());
	}
	else {
		ec.clear();
		CloseHandle(p.proc_info.hProcess);
		p.proc_info.hProcess = INVALID_HANDLE_VALUE;
	}
};

inline bool is_running(int code) {
	return code == still_active;
}

inline bool is_running(const child_handle& p, int& exit_code, std::error_code& ec) {
	DWORD code;
	if (!GetExitCodeProcess(p.process_handle(), &code)) {
		ec = std::error_code(GetLastError(), std::system_category());
	}
	else {
		ec.clear();
	}

	if (code == still_active) {
		return true;
	}
	else {
		exit_code = code;
		return false;
	}
}

}
}
