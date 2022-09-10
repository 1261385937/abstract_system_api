#pragma once
#include <memory>
#include <chrono>
#if _WIN32
#include "platform/windows/process_handle.hpp"
#include "platform/windows/process_action.hpp"

namespace asa {
namespace api = windows;
}

#else

namespace asa {
namespace api = posix;
}
#endif

namespace asa {
class child {
public:
	using child_handle = api::child_handle;
	using native_handle_t = child_handle::process_handle_t;
	using pid_t = api::pid_t;

private:
	api::child_handle handle_;
	std::atomic<int> exit_status_ = api::still_active;
	bool attached_ = true;
	bool terminated_ = false;

public:
	child(const child&) = delete;
	child& operator=(const child&) = delete;

	child(child&& lhs) noexcept
		: handle_(std::move(lhs.handle_)),
		exit_status_(lhs.exit_status_.load()),
		attached_(lhs.attached_),
		terminated_(lhs.terminated_) 
	{
		lhs.attached_ = false;
	}

	child& operator=(child&& lhs) noexcept {
		handle_ = std::move(lhs.handle_);
		exit_status_ = lhs.exit_status_.load();
		attached_ = lhs.attached_;
		terminated_ = lhs.terminated_;
		lhs.attached_ = false;
		return *this;
	};

	~child() {
		std::error_code ec;
		if (attached_ && !exited() && running(ec)) {
			terminate(ec);
		}
	}

	explicit child(pid_t pid) : handle_(pid), attached_(false) {};

	template<typename ...Args>
	explicit child(std::string_view execute, Args&&...args) {
		handle_ = api::start_process(execute, std::forward<Args>(args)...);
	}

	void detach() { attached_ = false; }

	pid_t id() const { return handle_.id(); }

	bool valid() const { return handle_.valid(); }

	operator bool() const { return valid(); }

	int exit_code() const { return exit_status_.load(); }

	int native_exit_code() const { 
		return exit_status_.load(); 
	}

	native_handle_t native_handle() const {
		return handle_.process_handle();
	}

	bool running(std::error_code& ec) noexcept {
		ec.clear();
		if (valid() && !exited() && !ec) {
			int exit_code = 0;
			auto res = api::is_running(handle_, exit_code, ec);
			if (!ec && !res && !exited()) {
				exit_status_.store(exit_code);
			}
			return res;
		}
		return false;
	}

	void terminate(std::error_code& ec) noexcept {
		if (valid() && running(ec) && !ec) {
			api::terminate_process(handle_, ec);
		}
		if (!ec) {
			terminated_ = true;
		}
	}

private:
	bool exited() {
		return terminated_ || !api::is_running(exit_status_.load());
	};
};

}
