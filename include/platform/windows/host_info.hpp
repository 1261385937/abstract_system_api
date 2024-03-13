#pragma once
#include <string>
#include <cstdint>
#include <codecvt>
#include <vector>
#include <set>
#include <unordered_map>
#include <system_error>

#pragma warning(disable: 4996)
#pragma comment(lib, "IPHLPAPI.lib")
#pragma comment(lib,"ws2_32.lib")

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <winsock2.h>
#include<WS2tcpip.h>
#include <iphlpapi.h>

#include "host_handle.hpp"

namespace asa {
namespace windows {

inline std::string get_hostname(std::error_code& ec) {
    ec.clear();

    DWORD size = MAX_COMPUTERNAME_LENGTH + 1;
    wchar_t buf[MAX_COMPUTERNAME_LENGTH + 1]{};
    auto ret = GetComputerNameW(buf, &size);
    if (ret == 0) { //error
        ec = std::error_code(GetLastError(), std::system_category());
        return {};
    }
    std::wstring_convert<std::codecvt_utf8<wchar_t>> cvt;
    auto name = cvt.to_bytes(buf);
    return name;
}

inline std::string get_os_info(std::error_code& ec)
{
    ec.clear();
    std::string os_info = "other";
    HINSTANCE hinst = LoadLibraryA("ntdll.dll");
    if (hinst == nullptr) {
        ec = std::error_code(GetLastError(), std::system_category());
        return os_info;
    }

    typedef void(__stdcall* NTPROC)(DWORD*, DWORD*, DWORD*);
    NTPROC proc = (NTPROC)GetProcAddress(hinst, "RtlGetNtVersionNumbers");
    FreeLibrary(hinst);
    if (proc == nullptr) {
        ec = std::error_code(GetLastError(), std::system_category());
        return os_info;
    }

    DWORD dwMajor, dwMinor, dwBuildNumber;
    proc(&dwMajor, &dwMinor, &dwBuildNumber);
    if (dwMajor == 6 && dwMinor == 3) {
        if (dwBuildNumber == 4026541440) {
            os_info = "windows server 2012 R2";
            return os_info;
        }
    }
    else if (dwMajor == 10 && dwMinor == 0) {
        if (dwBuildNumber == 4026546233) {
            os_info = "windows server 2016";
        }
        else if (dwBuildNumber == 4026553840) {
            os_info = "windows 11";
        }
        else {
            os_info = "windows 10";
        }
        return os_info;
    }

    //RtlGetNtVersionNumbers no get the os_info
    OSVERSIONINFOEX os{};
    os.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
    if (!GetVersionEx((OSVERSIONINFO*)&os)) {
        ec = std::error_code(GetLastError(), std::system_category());
        return os_info;
    }
    if (os.dwMajorVersion != 6) {
        return os_info;
    }
    if (os.dwMinorVersion == 0 && os.wProductType != VER_NT_WORKSTATION) {
        os_info = "windows server 2008";
        return os_info;
    }
    if (os.dwMinorVersion == 1) {
        os_info =
            os.wProductType == VER_NT_WORKSTATION ? "windows 7" : "windows server 2008 R2";
        return os_info;
    }
    return os_info;
}

inline cpu_occupy get_cpu_occupy(std::error_code& ec)
{
    ec.clear();
    auto filetime_to_uint64_t = [](const FILETIME& time) {
        return (uint64_t)time.dwHighDateTime << 32 | time.dwLowDateTime;
    };

    FILETIME idle_time{};
    FILETIME kernel_time{};
    FILETIME user_time{};

    auto ok = GetSystemTimes(&idle_time, &kernel_time, &user_time);
    if (!ok) {
        ec = std::error_code(GetLastError(), std::system_category());
        return {};
    }

    cpu_occupy occupy{};
    occupy.idle_time = filetime_to_uint64_t(idle_time);
    occupy.kernel_time = filetime_to_uint64_t(kernel_time);
    occupy.user_time = filetime_to_uint64_t(user_time);
    return occupy;
}

inline int32_t calculate_cpu_usage(const cpu_occupy& pre, const cpu_occupy& now)
{
    auto idel_delta = now.idle_time - pre.idle_time;
    auto kernel_delta = now.kernel_time - pre.kernel_time;
    auto user_detal = now.user_time - pre.user_time;
    auto total_detal = kernel_delta + user_detal;
    if (total_detal == 0) {
        return 0;
    }

    auto cpu = (kernel_delta + user_detal - idel_delta) * 100.0 / total_detal;
    int32_t usage = static_cast<int32_t>(ceil(cpu));
    return usage;
}

inline int32_t get_memory_usage(std::error_code& ec)
{
    ec.clear();
    MEMORYSTATUSEX ex{};
    ex.dwLength = sizeof(ex);
    auto ok = GlobalMemoryStatusEx(&ex);
    if (!ok) {
        ec = std::error_code(GetLastError(), std::system_category());
        return 0;
    }
    return ex.dwMemoryLoad;
}

inline bool is_physics(const std::string& network_card_name) {
    char prefix[] = R"(SYSTEM\CurrentControlSet\Control\Network\{4D36E972-E325-11CE-BFC1-08002BE10318})";
    HKEY subKey = NULL;
    auto ret = RegOpenKeyExA(HKEY_LOCAL_MACHINE, prefix, 0, KEY_READ, &subKey);
    if (ret != ERROR_SUCCESS) {
        return false;
    }
    auto deleter = std::shared_ptr<char>(
        new char, [subKey](char* p) {delete p; RegCloseKey(subKey); });

    std::string conn = network_card_name + R"(\Connection)";
    HKEY localKey = NULL;
    ret = RegOpenKeyExA(subKey, conn.data(), 0, KEY_READ, &localKey);
    if (ERROR_SUCCESS != ret) {
        return false;
    }
    auto deleter1 = std::shared_ptr<char>(
        new char, [localKey](char* p) {delete p; RegCloseKey(localKey); });

    DWORD type = REG_SZ;
    char data[512]{};
    DWORD data_Size = sizeof(data);
    ret = RegQueryValueExA(localKey, "PnPInstanceId", 0, &type, (BYTE*)data, &data_Size);
    if (ERROR_SUCCESS != ret) {
        return false;
    }

    if (0 == strncmp(data, "PCI", 3) || 0 == strncmp(data, "USB", 3)) {
        return true;
    }
    return false;
}

inline network_card_t get_network_card(std::error_code& ec)
{
    ec.clear();
    // Allocate a 1MB buffer to start with.
    ULONG buf_len = 1024 * 1024;
    auto adapter_buf = std::shared_ptr<char>(new char[buf_len], [](char* p) {delete[] p; });

    auto buf = (IP_ADAPTER_ADDRESSES*)adapter_buf.get();
    auto ret = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX, NULL, buf, &buf_len);
    if (ret != ERROR_SUCCESS) {
        ec = std::error_code(GetLastError(), std::system_category());
        return {};
    }

    network_card_t cards;
    std::wstring_convert<std::codecvt_utf8<wchar_t>> cvt;
    auto currnet = buf;
    for (; currnet != nullptr; currnet = currnet->Next) {
        if (currnet->IfType != MIB_IF_TYPE_ETHERNET &&
            currnet->IfType != MIB_IF_TYPE_LOOPBACK &&
            currnet->IfType != IF_TYPE_IEEE80211) {
            continue;
        }

        networkcard card{};
        card.desc = cvt.to_bytes(currnet->Description);
        if (card.desc.find("Virtual Adapter") != std::string::npos) {
            continue;
        }
        card.is_down = (currnet->OperStatus == IF_OPER_STATUS::IfOperStatusDown);
        card.is_physics = is_physics(currnet->AdapterName);
        card.real_name = currnet->AdapterName;
        card.friend_name = cvt.to_bytes(currnet->FriendlyName);
        card.receive_speed = currnet->ReceiveLinkSpeed / 1000 / 1000;
        card.transmit_speed = currnet->TransmitLinkSpeed / 1000 / 1000;

        auto pUnicast = currnet->FirstUnicastAddress;
        for (; pUnicast != nullptr; pUnicast = pUnicast->Next) {
            if (pUnicast->Address.lpSockaddr->sa_family == AF_INET) {
                sockaddr_in* sa_in = (sockaddr_in*)pUnicast->Address.lpSockaddr;
                char buff[64]{};
                inet_ntop(AF_INET, &(sa_in->sin_addr), buff, 64);
                if (strncmp("169.254", buff, 7) == 0) { //filter Link-local address
                    continue;
                }
                card.ipv4.emplace(buff);
            }
            else if (pUnicast->Address.lpSockaddr->sa_family == AF_INET6) {
                sockaddr_in6* sa_in6 = (sockaddr_in6*)pUnicast->Address.lpSockaddr;
                char buff[64]{};
                inet_ntop(AF_INET6, &(sa_in6->sin6_addr), buff, 64);
                if (strncmp("fe80", buff, 4) == 0) { //filter Link-local address
                    continue;
                }
                card.ipv6.emplace(buff);
            }
            else {
                void(0);
            }
        }
        cards.emplace(card.real_name, std::move(card));
    }
    return cards;
}

template<typename C>
inline card_flow get_network_card_flow(C&& c, std::error_code& ec)
{
    ec.clear();
    if (c.empty()) {
        return {};
    }
    ULONG buf_len = 0;
    auto ret = GetIfTable(NULL, &buf_len, 0);
    auto iftable_buf = std::shared_ptr<char>(new char[buf_len], [](char* p) {delete[] p; });

    auto buf = (MIB_IFTABLE*)iftable_buf.get();
    ret = GetIfTable(buf, &buf_len, 0);
    if (NO_ERROR != ret) {
        ec = std::error_code(GetLastError(), std::system_category());
        return {};
    }

    card_flow flow;
    std::wstring_convert<std::codecvt_utf8<wchar_t>> cvt;
    auto if_table = buf;
    for (DWORD i = 0; i != if_table->dwNumEntries; ++i) {
        //\DEVICE\TCPIP_{9EFCFA8D-22C9-11ED-B6A1-806E6F6E6963}
        auto device_name = cvt.to_bytes(if_table->table[i].wszName);
        auto pos = device_name.find("{");
        if (pos == std::string::npos) {
            continue; //theoretically impossible
        }
        auto real_name = device_name.substr(pos);
        if (auto it = c.find(real_name); it == c.end()) {
            continue;// not want card
        }
        flow.emplace(std::move(real_name),
            std::pair{ if_table->table[i].dwInOctets, if_table->table[i].dwOutOctets });
    }
    return flow;
}

inline disk_info get_disk_info(std::string_view name, std::error_code& ec) {
    ec.clear();

    disk_info info{};
    auto ret = GetDiskFreeSpaceExA(name.data(),
        (PULARGE_INTEGER)&info.available_size,
        (PULARGE_INTEGER)&info.total_size,
        nullptr);
    if (ret == 0) {
        ec = std::error_code(GetLastError(), std::system_category());
        return {};
    }
    return info;
}

inline auto get_toute_table(std::error_code& ec) {
    ec.clear();
    std::vector<std::vector<std::string>> tables;
    return tables;
}

}
}
