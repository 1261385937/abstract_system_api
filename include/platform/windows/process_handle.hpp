#pragma once
#include <system_error>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace asa {
namespace windows {

using pid_t = DWORD;

struct child_handle {
	PROCESS_INFORMATION proc_info{};

	child_handle() = default;
	child_handle(const child_handle& c) = delete;
	child_handle& operator=(const child_handle& c) = delete;

	explicit child_handle(const PROCESS_INFORMATION& pi)
		: proc_info(pi) {}

	explicit child_handle(pid_t pid) {
		auto h = OpenProcess(PROCESS_ALL_ACCESS, 0, pid);
		if (h == nullptr) {
			throw std::system_error(
				std::error_code(GetLastError(), std::system_category()), "OpenProcess failed");
		}
		proc_info.hProcess = h;
		proc_info.dwProcessId = pid;
	}

	~child_handle() {
		CloseHandle(proc_info.hProcess);
		CloseHandle(proc_info.hThread);
	}


	child_handle(child_handle&& c) noexcept
		: proc_info(c.proc_info) {
		c.proc_info.hProcess = INVALID_HANDLE_VALUE;
		c.proc_info.hThread = INVALID_HANDLE_VALUE;
	}

	child_handle& operator=(child_handle&& c) noexcept {
		CloseHandle(proc_info.hProcess);
		CloseHandle(proc_info.hThread);

		proc_info = c.proc_info;
		c.proc_info.hProcess = INVALID_HANDLE_VALUE;
		c.proc_info.hThread = INVALID_HANDLE_VALUE;
		return *this;
	}

	pid_t id() const {
		return static_cast<pid_t>(proc_info.dwProcessId);
	}

	using process_handle_t = HANDLE;
	process_handle_t process_handle() const {
		return proc_info.hProcess;
	}

	bool valid() const {
		return (proc_info.hProcess != nullptr) &&
			(proc_info.hProcess != INVALID_HANDLE_VALUE);
	}
};

}
}

