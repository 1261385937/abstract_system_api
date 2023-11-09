#pragma once
#include <string>
#include <tuple>
#include <system_error>
#include <memory>
#include "process_handle.hpp"

#include <sys/types.h> 
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>

namespace asa {
namespace aix {

constexpr int still_active = 0x017f;

static_assert(WIFSTOPPED(still_active), "Expected still_active to indicate WIFSTOPPED");
static_assert(!WIFEXITED(still_active), "Expected still_active to not indicate WIFEXITED");
static_assert(!WIFSIGNALED(still_active), "Expected still_active to not indicate WIFSIGNALED");
static_assert(!WIFCONTINUED(still_active), "Expected still_active to not indicate WIFCONTINUED");

template<typename F, std::size_t ... Index>
inline constexpr void for_each_tuple(F&& f, std::index_sequence<Index...>) {
    (std::forward<F>(f)(std::integral_constant<std::size_t, Index>()), ...);
}

template<bool parent_death_sig = false, typename ...Args>
inline child_handle start_process(std::string_view execute, Args&&... args) {
    const char* cmd_lines[] = { std::forward<Args>(args)..., nullptr };

    auto pid = vfork();
    if (pid == -1) {
        throw std::system_error(
            std::error_code(errno, std::system_category()), "vfork failed");
    }
    else if (pid == 0) {
        char** env = environ;
        execve(execute.data(), const_cast<char**>(cmd_lines), env);
        _exit(EXIT_FAILURE);
    }

    return child_handle(pid);
};

inline void terminate_process(child_handle& p, std::error_code& ec) {
    if (kill(p.pid, SIGKILL) == -1) {
        ec = std::error_code(errno, std::system_category());
    }
    else {
        ec.clear();
    }

    //should not be WNOHANG, since that would allow zombies.
    int status;
    waitpid(p.pid, &status, 0);
};

inline bool is_running(int code) {
    return !WIFEXITED(code) && !WIFSIGNALED(code);
}

inline bool is_running(const child_handle& p, int& exit_code, std::error_code& ec) {
    int status;
    auto ret = waitpid(p.pid, &status, WNOHANG);

    if (ret == 0) { //running
        return true;
    }

    if (ret == -1) { //error
        ec = (errno != ECHILD) ?
            std::error_code(errno, std::system_category()) :
            std::error_code{};
        return false;
    }

    //exited
    ec.clear();
    if (!is_running(status)) {
        exit_code = status;
    }
    return false;
}

inline void wait(child_handle& p, int& exit_code, std::error_code& ec) {
    pid_t ret = 0;
    int status = 0;

    do {
        ret = waitpid(p.pid, &status, 0);
    } while (((ret == -1) && (errno == EINTR)) ||
        (ret != -1 && !WIFEXITED(status) && !WIFSIGNALED(status)));

    if (ret == -1) {
        ec = std::error_code(errno, std::system_category());
    }
    else {
        ec.clear();
        exit_code = status;
    }
}

}
}
