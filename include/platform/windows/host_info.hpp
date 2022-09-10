#pragma once
#include <string>
#include <cstdint>
#include <codecvt>
#include <vector>
#include <set>
#include <unordered_map>

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

inline std::string get_hostname() {
	DWORD size = MAX_COMPUTERNAME_LENGTH + 1;
	char buf[MAX_COMPUTERNAME_LENGTH + 1]{};
	GetComputerNameA(buf, &size);
	return std::string(buf);
}

inline std::string get_os_info()
{
	std::string os_info = "unknown system";
	typedef void(__stdcall* NTPROC)(DWORD*, DWORD*, DWORD*);
	HINSTANCE hinst = LoadLibraryA("ntdll.dll");
	if (hinst == nullptr) {
		return os_info;
	}

	DWORD dwMajor, dwMinor, dwBuildNumber;
	NTPROC proc = (NTPROC)GetProcAddress(hinst, "RtlGetNtVersionNumbers");
	proc(&dwMajor, &dwMinor, &dwBuildNumber);
	FreeLibrary(hinst);
	if (dwMajor == 6 && dwMinor == 3) {
		if (dwBuildNumber == 4026541440) {
			os_info = "windows server 2012 R2";
			return os_info;
		}
	}
	else if (dwMajor == 10 && dwMinor == 0) {
		os_info = dwBuildNumber == 4026546233 ? "windows server 2016" : "windows 10";
		return os_info;
	}

	//RtlGetNtVersionNumbers no get the os_info
	OSVERSIONINFOEX os{};
	os.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
	if (!GetVersionEx((OSVERSIONINFO*)&os)) {
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

inline bool get_cpu_occupy(cpu_occupy& occupy)
{
	auto filetime_to_uint64_t = [](const FILETIME& time) {
		return (uint64_t)time.dwHighDateTime << 32 | time.dwLowDateTime;
	};

	FILETIME idle_time{};
	FILETIME kernel_time{};
	FILETIME user_time{};

	auto ok = GetSystemTimes(&idle_time, &kernel_time, &user_time);
	if (!ok) {
		return false;
	}
	occupy.idle_time = filetime_to_uint64_t(idle_time);
	occupy.kernel_time = filetime_to_uint64_t(kernel_time);
	occupy.user_time = filetime_to_uint64_t(user_time);
	return true;
}

inline int32_t calculate_cpu_usage(const cpu_occupy& pre, const cpu_occupy& now)
{
	auto pre_idle = pre.idle_time;
	auto pre_sys = pre.kernel_time + pre.user_time;

	auto now_idle = now.idle_time;
	auto now_sys = now.kernel_time + now.user_time;

	auto idel_delta = now_idle - pre_idle;
	auto sys_delta = now_sys - pre_sys;
	int32_t usage = static_cast<int32_t>
		(ceil((double)100.0 * (sys_delta - idel_delta)) / sys_delta);
	return usage;
}

inline int32_t get_memory_usage()
{
	MEMORYSTATUSEX ex{};
	ex.dwLength = sizeof(ex);
	auto ok = GlobalMemoryStatusEx(&ex);
	if (!ok) {
		return -1;
	}
	return ex.dwMemoryLoad;
}

inline network_card_t get_network_card()
{
	ULONG buf_len = 32 * 1024;
	auto buf = new char[buf_len]; // Allocate a 32 KB buffer to start with.
	auto deleter = std::shared_ptr<int>(new int, [buf](int* p) {delete p; delete[] buf; });
	auto ret = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX, NULL,
		(IP_ADAPTER_ADDRESSES*)buf, &buf_len);
	if (ret != NO_ERROR) {
		return {};
	}

	network_card_t cards;
	std::wstring_convert<std::codecvt_utf8<wchar_t>> cvt;
	auto currnet = (IP_ADAPTER_ADDRESSES*)buf;
	for (; currnet != nullptr; currnet = currnet->Next) {
		networkcard card{};
		card.is_down = (currnet->OperStatus == IF_OPER_STATUS::IfOperStatusDown);
		card.real_name = currnet->AdapterName;
		card.friend_name = cvt.to_bytes(currnet->FriendlyName);
		card.desc = cvt.to_bytes(currnet->Description);
		card.recive_speed = currnet->ReceiveLinkSpeed / 1000 / 1000;
		card.transmit_speed = currnet->TransmitLinkSpeed / 1000 / 1000;

		auto pUnicast = currnet->FirstUnicastAddress;
		for (; pUnicast != nullptr; pUnicast = pUnicast->Next) {
			if (pUnicast->Address.lpSockaddr->sa_family == AF_INET) {
				sockaddr_in* sa_in = (sockaddr_in*)pUnicast->Address.lpSockaddr;
				char buff[64]{};
				card.ipv4.emplace(inet_ntop(AF_INET, &(sa_in->sin_addr), buff, 64));
			}
			else if (pUnicast->Address.lpSockaddr->sa_family == AF_INET6) {
				sockaddr_in6* sa_in6 = (sockaddr_in6*)pUnicast->Address.lpSockaddr;
				char buff[64]{};
				card.ipv6.emplace(inet_ntop(AF_INET6, &(sa_in6->sin6_addr), buff, 64));
			}
			else {
				void(0);
			}
		}
		cards.emplace(card.real_name, std::move(card));
	}
	return cards;
}

inline card_flow get_network_card_flow(const card_name& names)
{
    if (names.empty()) {
        return {};
    }

	ULONG buffer_len = 0;
	auto ret = GetIfTable(NULL, &buffer_len, 0);
	auto buf = new char[buffer_len];
	auto deleter = std::shared_ptr<int>(new int, [buf](int* p) {delete p; delete[] buf; });
	ret = GetIfTable((MIB_IFTABLE*)buf, &buffer_len, 0);
	if (NO_ERROR != ret) {
		return {};
	}

	card_flow flow;
	std::wstring_convert<std::codecvt_utf8<wchar_t>> cvt;
	auto if_table = (MIB_IFTABLE*)buf;
	for (DWORD i = 0; i != if_table->dwNumEntries; ++i) {
		//\DEVICE\TCPIP_{9EFCFA8D-22C9-11ED-B6A1-806E6F6E6963}
		auto device_name = cvt.to_bytes(if_table->table[i].wszName);
		auto pos = device_name.find("{");
		if (pos == std::string::npos) {
			continue; //theoretically impossible
		}
		auto real_name = device_name.substr(pos);
		if (auto it = names.find(real_name); it == names.end()) {
			continue;// not want card
		}
		flow.emplace(std::move(real_name),
			std::pair{ if_table->table[i].dwInOctets, if_table->table[i].dwOutOctets });
	}
	return flow;
}

}
}
