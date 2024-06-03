#pragma once
#include <string>
#include <tuple>
#include <system_error>
#include <memory>
#include <thread>
#include "process_handle.hpp"

#include <sys/types.h> 
#include <sys/wait.h>
#include <unistd.h>
#include <sys/prctl.h>
#include <signal.h>

namespace asa {
namespace posix {

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
inline child_handle start_process(Args&&... args) {
    //const char* cmd_lines[] = { std::forward<Args>(args)..., nullptr };
    auto tup = std::forward_as_tuple(std::forward<Args>(args)...);
    constexpr auto tup_size = std::tuple_size_v<decltype(tup)>;

    std::vector<std::string> parameters;
    parameters.reserve(tup_size);
    for_each_tuple([&parameters, &tup, &tup_size](auto index) {
        using type = decltype(std::get<index>(tup));
        if constexpr (std::is_convertible_v<type, std::string>) {
            parameters.emplace_back(std::get<index>(tup));
        }
        else {
            parameters.emplace_back(std::to_string(std::get<index>(tup)));
        }
    }, std::make_index_sequence<tup_size>());

    std::vector<const char*> argv;
    for (auto&& parameter : parameters) {
        argv.push_back(parameter.c_str());
    }
    argv.push_back(nullptr);

    auto pid = vfork();
    if (pid == -1) {
        throw std::system_error(
            std::error_code(errno, std::system_category()), "vfork failed");
    }
    else if (pid == 0) {
        if constexpr (parent_death_sig) {
            prctl(PR_SET_PDEATHSIG, SIGKILL);
        }

        char** env = environ;
        execve(argv[0], const_cast<char* const*>(argv.data()), env);
        _exit(EXIT_FAILURE);
    }

    return child_handle(pid);
}

inline void terminate_process(child_handle& p, std::error_code& ec) {
    int status;
    if (kill(p.pid, SIGTERM) != -1) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        auto ret = waitpid(p.pid, &status, WNOHANG);
        if (ret > 0) {
            return;
        }
    }
    
    if (kill(p.pid, SIGKILL) == -1) {
        ec = std::error_code(errno, std::system_category());
    }  
    else {
        ec.clear();
    }
        
    //should not be WNOHANG, since that would allow zombies.
    waitpid(p.pid, &status, 0); 
}

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
