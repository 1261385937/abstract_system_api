#pragma once

namespace asa {
namespace posix {

using pid_t = int;

struct child_handle {
    pid_t pid{-1};

    child_handle() = default;
    ~child_handle() = default;
    child_handle(const child_handle& c) = delete;
    child_handle& operator=(const child_handle& c) = delete;

    explicit child_handle(pid_t pid) : pid(pid) {}

    child_handle(child_handle&& c) : pid(c.pid) { c.pid = -1; }

    child_handle& operator=(child_handle&& c) {
        pid = c.pid;
        c.pid = -1;
        return *this;
    }

    int id() const { return pid; }

    using process_handle_t = int;
    process_handle_t process_handle() const { return pid; }

    bool valid() const { return pid != -1; }
};
}  // namespace posix
}  // namespace asa
