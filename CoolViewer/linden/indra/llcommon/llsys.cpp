/**
 * @file llsys.cpp
 * @brief Impelementation of the basic system query functions.
 *
 * $LicenseInfo:firstyear=2002&license=viewergpl$
 *
 * Copyright (c) 2002-2009, Linden Research, Inc.
 *
 * Second Life Viewer Source Code
 * The source code in this file ("Source Code") is provided by Linden Lab
 * to you under the terms of the GNU General Public License, version 2.0
 * ("GPL"), unless you have obtained a separate licensing agreement
 * ("Other License"), formally executed by you and Linden Lab.  Terms of
 * the GPL can be found in doc/GPL-license.txt in this distribution, or
 * online at http://secondlifegrid.net/programs/open_source/licensing/gplv2
 *
 * There are special exceptions to the terms and conditions of the GPL as
 * it is applied to this Source Code. View the full text of the exception
 * in the file doc/FLOSS-exception.txt in this software distribution, or
 * online at
 * http://secondlifegrid.net/programs/open_source/licensing/flossexception
 *
 * By copying, modifying or distributing this software, you acknowledge
 * that you have read and understood your obligations described above,
 * and agree to abide by those obligations.
 *
 * ALL LINDEN LAB SOURCE CODE IS PROVIDED "AS IS." LINDEN LAB MAKES NO
 * WARRANTIES, EXPRESS, IMPLIED OR OTHERWISE, REGARDING ITS ACCURACY,
 * COMPLETENESS OR PERFORMANCE.
 * $/LicenseInfo$
 */

#include "linden_common.h"

#include <sstream>

#if LL_WINDOWS
# include <psapi.h>
# include <VersionHelpers.h>
#elif LL_DARWIN
# include <errno.h>
# include <sys/sysctl.h>
# include <sys/utsname.h>
# include <stdint.h>
# include <CoreServices/CoreServices.h>
# include <mach/task.h>
// Disable warnings about Gestalt calls being deprecated until Apple gets on
// the ball and provides an alternative
# pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#elif LL_LINUX
# include <errno.h>
# include <sched.h>				// For sched_setaffinity()
# include <sys/utsname.h>
# include <unistd.h>
#endif

#include "llsys.h"

#include "llmemory.h"
#include "llprocessor.h"
#include "llthread.h"
#include "lltimer.h"

///////////////////////////////////////////////////////////////////////////////
// LLOSInfo class
///////////////////////////////////////////////////////////////////////////////

#if LL_WINDOWS

// We cannot trust GetVersionEx function on Win8.1; we should check this value
// when creating OS string
constexpr U32 WINNT_WINBLUE = 0x0603;

#ifndef DLLVERSIONINFO
typedef struct _DllVersionInfo
{
    DWORD cbSize;
    DWORD dwMajorVersion;
    DWORD dwMinorVersion;
    DWORD dwBuildNumber;
    DWORD dwPlatformID;
} DLLVERSIONINFO;
#endif

#ifndef DLLGETVERSIONPROC
typedef int (FAR WINAPI* DLLGETVERSIONPROC) (DLLVERSIONINFO*);
#endif

bool get_shell32_dll_version(DWORD& major, DWORD& minor, DWORD& build_number)
{
	bool result = false;

	constexpr U32 BUFF_SIZE = 32767;
	WCHAR tempBuf[BUFF_SIZE];
	if (GetSystemDirectory((LPWSTR)&tempBuf, BUFF_SIZE))
	{
		std::basic_string<WCHAR> shell32_path(tempBuf);

		// Shell32.dll contains the DLLGetVersion function. According to MSDN
		// it is not part of the API so you have to go in and get it.
		// http://msdn.microsoft.com/en-us/library/bb776404(VS.85).aspx
		shell32_path += TEXT("\\shell32.dll");

		// Try and load the DLL
		HMODULE h_dll_inst = LoadLibrary(shell32_path.c_str());
		if (h_dll_inst)	// Could successfully load the DLL
		{
			DLLGETVERSIONPROC dll_get_verp;
			// You must get this function explicitly because earlier versions
			// of the DLL do not implement this function. That makes the lack
			// of implementation of the function a version marker in itself.
			dll_get_verp = (DLLGETVERSIONPROC)GetProcAddress(h_dll_inst,
															 "DllGetVersion");
			if (dll_get_verp)
			{
				// DLL supports version retrieval function
				DLLVERSIONINFO dvi;
				ZeroMemory(&dvi, sizeof(dvi));
				dvi.cbSize = sizeof(dvi);
				HRESULT hr = (*dll_get_verp)(&dvi);
				if (SUCCEEDED(hr))
				{
					// Finally, the version is at our hands
					major = dvi.dwMajorVersion;
					minor = dvi.dwMinorVersion;
					build_number = dvi.dwBuildNumber;
					result = true;
				}
			}

			FreeLibrary(h_dll_inst);  // Release the DLL
		}
	}

	return result;
}

// GetVersionEx does not work correctly with Windows 8.1 and newer versions.
// We need to check this case.
static bool	check_for_version(WORD wMajorVersion, WORD wMinorVersion,
							  WORD wServicePackMajor)
{
	OSVERSIONINFOEXW osvi = { sizeof(osvi), 0, 0, 0, 0, { 0 }, 0, 0 };
	DWORDLONG const dwlConditionMask =
		VerSetConditionMask(VerSetConditionMask(VerSetConditionMask(0,
																	VER_MAJORVERSION,
																	VER_GREATER_EQUAL),
												VER_MINORVERSION, VER_GREATER_EQUAL),
							VER_SERVICEPACKMAJOR, VER_GREATER_EQUAL);

	osvi.dwMajorVersion = wMajorVersion;
	osvi.dwMinorVersion = wMinorVersion;
	osvi.wServicePackMajor = wServicePackMajor;

	return VerifyVersionInfoW(&osvi,
							  VER_MAJORVERSION | VER_MINORVERSION |
							  VER_SERVICEPACKMAJOR,
							  dwlConditionMask) != FALSE;
}

#endif // LL_WINDOWS

LLOSInfo::LLOSInfo()
{
#if LL_WINDOWS
	bool should_use_shell_version = true;
	S32 major = 0;
	S32 minor = 0;
	S32 build = 0;

	// Try calling GetVersionEx using the OSVERSIONINFOEX structure.
	OSVERSIONINFOEX osvi;
	ZeroMemory(&osvi, sizeof(OSVERSIONINFOEX));
	osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
	bool success = GetVersionEx((OSVERSIONINFO*)&osvi) != 0;
	if (!success)
	{
		// If OSVERSIONINFOEX does not work, try OSVERSIONINFO.
		osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
		success = GetVersionEx((OSVERSIONINFO*)&osvi) != 0;
	}
	if (success)
	{
		major = osvi.dwMajorVersion;
		minor = osvi.dwMinorVersion;
		build = osvi.dwBuildNumber & 0xffff;
		llinfos << "Windows version as reported by GetVersionEx() would be: "
				<< major << "." << minor << "." << build << llendl;
	}
	else
	{
		llwarns << "Could not get Windows version via GetVersionEx()."
				<< llendl;
	}

	mOSStringSimple = "Windows unsupported version ";
	if (IsWindowsVistaSP2OrGreater() && major == 6 && minor <= 2)
	{
		if (minor == 0)
		{
			if (osvi.wProductType == VER_NT_WORKSTATION)
			{
				mOSStringSimple = "Windows Vista ";
			}
			else
			{
				mOSStringSimple = "Windows Server 2008 ";
			}
		}
		else if (minor == 1)
		{
			if (osvi.wProductType == VER_NT_WORKSTATION)
			{
				mOSStringSimple = "Windows 7 ";
			}
			else
			{
				mOSStringSimple = "Windows Server 2008 R2 ";
			}
		}
		else if (minor == 2)
		{
			if (check_for_version(HIBYTE(WINNT_WINBLUE),
								  LOBYTE(WINNT_WINBLUE), 0))
			{
				mOSStringSimple = "Windows 8.1 ";
				should_use_shell_version = false;
			}
			else if (osvi.wProductType == VER_NT_WORKSTATION)
			{
				mOSStringSimple = "Windows 8 ";
			}
			else
			{
				mOSStringSimple = "Windows Server 2012 ";
			}
		}
	}

	DWORD shell32_major, shell32_minor, shell32_build;
	if (should_use_shell_version &&
		get_shell32_dll_version(shell32_major, shell32_minor, shell32_build))
	{
		llinfos << "Windows shell version is: " << shell32_major << "."
				<< shell32_minor << "." << shell32_build << llendl;
		if (shell32_major > major)
		{
			major = shell32_major;
			minor = shell32_minor;
			build = shell32_build;
		}
		if (major >= 10)
		{
			if (major == 10 && build >= 22000)
			{
				mOSStringSimple = "Windows 10/11 ";
			}
			else
			{
				mOSStringSimple = llformat("Windows %d ", major);
			}
		}
	}

	DWORD revision = 0;
	if (major >= 10)
	{
		// For Windows 10+, get the update build revision from the registry
		HKEY key;
		if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
						  TEXT("SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion"),
						  0, KEY_READ, &key) == ERROR_SUCCESS)
		{
			DWORD cb_data(sizeof(DWORD));
			if (RegQueryValueExW(key, L"UBR", 0, NULL,
								 reinterpret_cast<LPBYTE>(&revision),
								 &cb_data) != ERROR_SUCCESS)
			{
				revision = 0;
			}
		}
	}
	if (revision)
	{
		mOSVersionString = llformat("%d.%d (build %d.%d)", major, minor, build,
									(S32)revision);
	}
	else
	{
		mOSVersionString = llformat("%d.%d (build %d)", major, minor, build);
	}
	mOSStringSimple += "64 bits ";
	mOSString = "Microsoft " + mOSStringSimple + "v" + mOSVersionString;
#elif LL_DARWIN
	// Initialize mOSStringSimple to something like:
	// "Mac OS X 10.6.7"
	{
		SInt32 major_version, minor_version, bugfix_version;
		OSErr r1 = Gestalt(gestaltSystemVersionMajor, &major_version);
		OSErr r2 = Gestalt(gestaltSystemVersionMinor, &minor_version);
		OSErr r3 = Gestalt(gestaltSystemVersionBugFix, &bugfix_version);

		if (r1 == noErr && r2 == noErr && r3 == noErr)
		{
			mOSVersionString = llformat("%d.%d.%d", major_version,
										minor_version, bugfix_version);

			std::string os_version = "Mac OS X " + mOSVersionString;
			// Put it in the OS string we are compiling
			mOSStringSimple.append(os_version);
		}
		else
		{
			mOSStringSimple.append("Unable to collect OS info");
		}
	}

	// Initialize mOSString to something like:
	// "Mac OS X 10.6.7 Darwin Kernel Version 10.7.0:
	// Sat Jan 29 15:17:16 PST 2011; root:xnu-1504.9.37~1/RELEASE_I386 i386"
	struct utsname un;
	if (uname(&un) != -1)
	{
		mOSString = mOSStringSimple;
		mOSString.append(" ");
		mOSString.append(un.sysname);
		mOSString.append(" ");
		mOSString.append(un.release);
		mOSString.append(" ");
		mOSString.append(un.version);
		mOSString.append(" ");
		mOSString.append(un.machine);
	}
	else
	{
		mOSString = mOSStringSimple;
	}
#else	// LL_LINUX
	mVersionMajor = mVersionMinor = 0;
	struct utsname un;
	if (uname(&un) != -1)
	{
		mOSString.assign(un.sysname);
		mOSString.append("-");
		mOSString.append(un.machine);
		mOSString.append(" v");
		mOSString.append(un.release);

		mOSStringSimple = mOSString;

		mOSVersionString.assign(un.version);
		mOSString.append(" ");
		mOSString.append(mOSVersionString);

		std::string version(un.release);
		size_t i = version.find('.');
		if (i != std::string::npos)
		{
			mVersionMajor = atoi(version.substr(0, i).c_str());
			version.erase(0, i + 1);
			i = version.find('.');
			if (i != std::string::npos)
			{
				mVersionMinor = atoi(version.substr(0, i).c_str());
			}
		}
	}
	else
	{
		mOSStringSimple = "Unable to collect OS info";
		mOSString = mOSStringSimple;
	}
#endif
}

#if LL_LINUX
constexpr S32 STATUS_SIZE = 8192;
#endif

//static
U32 LLOSInfo::getProcessVirtualSizeKB()
{
	U32 virtual_size = 0;
#if LL_LINUX
	LLFILE* status_filep = LLFile::open("/proc/self/status", "rb");
	if (status_filep)
	{
		char buff[STATUS_SIZE];

		size_t nbytes = fread(buff, 1, STATUS_SIZE - 1, status_filep);
		buff[nbytes] = '\0';

		// All these guys return numbers in KB
		U32 temp = 0;
		char* memp = strstr(buff, "VmRSS:");
		if (memp)
		{
			sscanf(memp, "%*s %u", &temp);
			virtual_size = temp;
		}
		memp = strstr(buff, "VmStk:");
		if (memp)
		{
			sscanf(memp, "%*s %u", &temp);
			virtual_size += temp;
		}
		memp = strstr(buff, "VmExe:");
		if (memp)
		{
			sscanf(memp, "%*s %u", &temp);
			virtual_size += temp;
		}
		memp = strstr(buff, "VmLib:");
		if (memp)
		{
			sscanf(memp, "%*s %u", &temp);
			virtual_size += temp;
		}
		memp = strstr(buff, "VmPTE:");
		if (memp)
		{
			sscanf(memp, "%*s %u", &temp);
			virtual_size += temp;
		}
		LLFile::close(status_filep);
	}
#elif LL_DARWIN
	struct task_basic_info t_info;
	mach_msg_type_number_t t_info_count = TASK_BASIC_INFO_COUNT;

	if (KERN_SUCCESS != task_info(mach_task_self(), TASK_BASIC_INFO,
								  (task_info_t)&t_info, &t_info_count))
	{
	  return 0;
	}
	virtual_size  = t_info.virtual_size / 1024;
#endif
	return virtual_size;
}

//static
U32 LLOSInfo::getProcessResidentSizeKB()
{
	U32 resident_size = 0;
#if LL_LINUX
	LLFILE* status_filep = LLFile::open("/proc/self/status", "rb");
	if (status_filep != NULL)
	{
		char buff[STATUS_SIZE];

		size_t nbytes = fread(buff, 1, STATUS_SIZE - 1, status_filep);
		buff[nbytes] = '\0';

		// All these guys return numbers in KB
		char* memp = strstr(buff, "VmRSS:");
		if (memp)
		{
			sscanf(memp, "%*s %u", &resident_size);
		}
		LLFile::close(status_filep);
	}
#elif LL_DARWIN
	struct task_basic_info t_info;
	mach_msg_type_number_t t_info_count = TASK_BASIC_INFO_COUNT;

	if (KERN_SUCCESS != task_info(mach_task_self(), TASK_BASIC_INFO,
								  (task_info_t)&t_info, &t_info_count))
	{
		return 0;
	}
	resident_size  = t_info.resident_size / 1024;
#endif
	return resident_size;
}

///////////////////////////////////////////////////////////////////////////////
// LLCPUInfo class
///////////////////////////////////////////////////////////////////////////////

// Static members
U32 LLCPUInfo::sMainThreadAffinityMask = 0;
bool LLCPUInfo::sMainThreadAffinitySet = false;

LLCPUInfo::LLCPUInfo()
:	mImpl(new LLProcessorInfo)
{
	mHasSSE2 = mImpl->hasSSE2();
	mCPUMHz = mImpl->getCPUFrequency();
	mFamily = mImpl->getCPUFamilyName();

	std::ostringstream out;
	out << mImpl->getCPUBrandName();
	// *NOTE: cpu speed is often way wrong, do a sanity check
	if (mCPUMHz > 200 && mCPUMHz < 10000)
	{
		out << " (" << mCPUMHz << " MHz)";
	}
	mCPUString = out.str();
}

LLCPUInfo::~LLCPUInfo()
{
	delete mImpl;
	mImpl = NULL;
}

std::string LLCPUInfo::getInfo() const
{
	std::ostringstream s;
	if (mImpl)
	{
		// Gather machine information.
		s << mImpl->getCPUFeatureDescription();

		// These are interesting as they reflect our internal view of the
		// CPU's attributes regardless of platform
		s << "->mHasSSE2:    " << (U32)mHasSSE2 << std::endl;
		s << "->mCPUMHz:     " << mCPUMHz << std::endl;
		s << "->mCPUString:  " << mCPUString << std::endl;
	}
	return s.str();
}

//static
void LLCPUInfo::setMainThreadCPUAffinifty(U32 cpu_mask)
{
	assert_main_thread(); // Must be called from the main thread only !

#if LL_LINUX
	sMainThreadAffinitySet = true;

	if (!cpu_mask)
	{
		return;
	}

	U32 vcpus = boost::thread::hardware_concurrency();
	U32 cores = boost::thread::physical_concurrency();
	if (vcpus < 4 || cores < 4)
	{
		llinfos << "Too few CPU cores to set an affinity. Skipping." << llendl;
		return;
	}

	U32 reserved_vcpus = 0;
	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	for (int i = 0, last = llmin((int)CPU_SETSIZE, 32); i < last; ++i)
	{
		if (cpu_mask & (1 << i))
		{
			CPU_SET(i, &cpuset);
			++reserved_vcpus;
		}
	}
	if (!reserved_vcpus)	// This should not happen (CPU_SETSIZE > 32)...
	{
		llwarns << "Request to reserve cores not part of available cores. Skipping."
				<< llendl;
		return;
	}
	if (reserved_vcpus + 2 > vcpus)
	{
		llwarns << "Request to reserve too many cores (" << reserved_vcpus
				<< ") for the main thread; only " << vcpus
				<< " cores are available on this system. Skipping." << llendl;
		return;
	}

	if (sched_setaffinity(0, sizeof(cpuset), &cpuset))
	{
		llwarns << "Failed to set CPU affinity for the main thread." << llendl;
		return;
	}

	// Success !  Remember our mask so to set a complementary one on each new
	// child thread.
	sMainThreadAffinityMask = cpu_mask;
#endif
}

//static
S32 LLCPUInfo::setThreadCPUAffinity()
{
#if LL_LINUX
	if (!sMainThreadAffinitySet)
	{
		// Cannot set a child thread affinity before the main thread one is set
		return -1;
	}

	// This is a no-operation if no affinity setting is used, or when called
	// from the main thread. Report a "success" in both cases.
	if (!sMainThreadAffinityMask || is_main_thread())
	{
		return 1;
	}

	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	for (U32 i = 0; i < (U32)CPU_SETSIZE; ++i)
	{
		if (i >= 32 || !(sMainThreadAffinityMask & (1 << i)))
		{
			CPU_SET((int)i, &cpuset);
		}
	}
	if (sched_setaffinity(0, sizeof(cpuset), &cpuset))
	{
		return 0;
	}
#endif
	return 1;
}

//static
F32 LLCPUInfo::benchmarkFactor()
{
	// Reference values for LLProcessorInfo::benchmark(), as measured on a
	// 9700K @ 5GHz (fixed, locked frequency on all cores) under Linux
	// (compiled with gcc, clang) and Windows (compiled with MSVC). HB
	constexpr U32 BENCH_REF_LIMIT = 10000000;
#if LL_CLANG
	constexpr F32 BENCH_REF_9700K_5GHZ = 33.8f;	// In ms
#elif LL_GNUC
	constexpr F32 BENCH_REF_9700K_5GHZ = 29.8f;	// In ms
#else	// LL_MSVC
	constexpr F32 BENCH_REF_9700K_5GHZ = 31.f;	// In ms
#endif

	// Let's average several benchmarks for a total duration of 300 to 600ms
	F64 total = 0.0;
	S32 iterations = 0;
	do
	{
		++iterations;
		total += LLProcessorInfo::benchmark(BENCH_REF_LIMIT);
	}
	while (total <= 300.0);

	F32 result = F32(total / F64(iterations));
	llinfos << "Time taken to find all prime numbers below " << BENCH_REF_LIMIT
			<< ": " << result << "ms." << llendl;

	F32 factor = BENCH_REF_9700K_5GHZ / result;
	llinfos << "CPU single-core performance factor relative to a 9700K @ 5GHz: "
			<< factor << llendl;

	return factor;
}

///////////////////////////////////////////////////////////////////////////////
// LLMemoryInfo class
///////////////////////////////////////////////////////////////////////////////

#if LL_LINUX
const char MEMINFO_FILE[] = "/proc/meminfo";
#endif

bool LLMemoryInfo::sAllowSwapping = false;

//static
U32 LLMemoryInfo::getPhysicalMemoryKB()
{
#if LL_WINDOWS
	MEMORYSTATUSEX state;
	state.dwLength = sizeof(state);
	GlobalMemoryStatusEx(&state);
	U32 amount = state.ullTotalPhys >> 10;

	// *HACK: for some reason, the reported amount of memory is always wrong.
	// The original adjustment assumes it is always off by one meg, however
	// errors of as much as 2520 KB have been observed in the value returned
	// from the GetMemoryStatusEx function.  Here we keep the original
	// adjustment from llfoaterabout.cpp until this can be fixed somehow. HB
	amount += 1024;

	return amount;
#elif LL_DARWIN
	// This might work on Linux as well. Someone check...
	uint64_t phys = 0;
	int mib[2] = { CTL_HW, HW_MEMSIZE };

	size_t len = sizeof(phys);
	sysctl(mib, 2, &phys, &len, NULL, 0);

	return (U32)(phys >> 10);
#elif LL_LINUX
	U64 phys = (U64)(sysconf(_SC_PAGESIZE)) * (U64)(sysconf(_SC_PHYS_PAGES));
	return (U32)(phys >> 10);
#else
	return 0;
#endif
}

//static
void LLMemoryInfo::getMaxMemoryKB(U32& max_physical_mem_kb,
								  U32& max_virtual_mem_kb)
{
	static bool saved_allow_swapping = false;
	static U32 saved_max_physical_mem_kb = 0;
	static U32 saved_max_virtual_mem_kb = 0;

	if (saved_allow_swapping == sAllowSwapping &&
		saved_max_virtual_mem_kb != 0)
	{
		max_physical_mem_kb = saved_max_physical_mem_kb;
		max_virtual_mem_kb = saved_max_virtual_mem_kb;
		return;
	}

	U32 addressable = U32_MAX;	// no limit...

#if LL_WINDOWS
	MEMORYSTATUSEX state;
	state.dwLength = sizeof(state);
	GlobalMemoryStatusEx(&state);
	max_physical_mem_kb = (U32)(state.ullAvailPhys / 1024);
	U32 total_virtual_memory = (U32)(state.ullTotalVirtual / 1024);
	if (sAllowSwapping)
	{
		max_virtual_mem_kb = total_virtual_memory;
	}
	else
	{
		max_virtual_mem_kb = llmin(total_virtual_memory, max_physical_mem_kb);
	}
#elif LL_LINUX || LL_DARWIN
	max_physical_mem_kb = getPhysicalMemoryKB();
	if (sAllowSwapping)
	{
		max_virtual_mem_kb = addressable;
	}
	else
	{
		max_virtual_mem_kb = max_physical_mem_kb;
	}
#else
	max_physical_mem_kb = max_virtual_mem_kb = addressable;
#endif
	max_virtual_mem_kb = llmin(max_virtual_mem_kb, addressable);

#if LL_WINDOWS
	LL_DEBUGS("Memory") << "Total physical memory: "
						<< max_physical_mem_kb / 1024
						<< "Mb - Total available virtual memory: "
						<< total_virtual_memory / 1024
						<< "Mb - Retained max virtual memory: "
						<< max_virtual_mem_kb / 1024 << "Mb" << LL_ENDL;
#else
	LL_DEBUGS("Memory") << "Total physical memory: "
						<< max_physical_mem_kb / 1024
						<< "Mb - Retained max virtual memory: "
						<< max_virtual_mem_kb / 1024 << "Mb" << LL_ENDL;
#endif

	saved_allow_swapping = sAllowSwapping;
	saved_max_physical_mem_kb = max_physical_mem_kb;
	saved_max_virtual_mem_kb = max_virtual_mem_kb;
}

//static
void LLMemoryInfo::getAvailableMemoryKB(U32& avail_physical_mem_kb,
										U32& avail_virtual_mem_kb)
{
	U32 max_physical_mem_kb, max_virtual_mem_kb;
	getMaxMemoryKB(max_physical_mem_kb, max_virtual_mem_kb);
	avail_physical_mem_kb = max_physical_mem_kb;

#if LL_WINDOWS

	MEMORYSTATUSEX state;
	state.dwLength = sizeof(state);
	GlobalMemoryStatusEx(&state);
	avail_virtual_mem_kb = (U32)(state.ullAvailVirtual / 1024);

# if 0
	LL_DEBUGS("Memory") << "Memory check: Retained available virtual space: "
						<< avail_virtual_mem_kb / 1024 << "Mb" << LL_ENDL;
# else
	// This is what Windows reports...
	U32 virtual_retained_kb = avail_virtual_mem_kb;

	// Let's try and compute something that reflects more accurately the
	// total memory consumption variations over time.
	HANDLE self = GetCurrentProcess();
	PROCESS_MEMORY_COUNTERS counters;
	if (GetProcessMemoryInfo(self, &counters, sizeof(counters)))
	{
		U32 virtual_size_kb = (U32)(counters.WorkingSetSize / 1024);
		// 20% penalty for fragmentation:
		virtual_size_kb += virtual_size_kb / 5;
		U32 virtual_computed_kb = 0;
		if (virtual_size_kb < max_virtual_mem_kb)
		{
			virtual_computed_kb = max_virtual_mem_kb - virtual_size_kb;
			U32 safety_margin_kb = LLMemory::getSafetyMarginKB();
#  if 1
			if (safety_margin_kb && avail_virtual_mem_kb > safety_margin_kb &&
				virtual_computed_kb > avail_virtual_mem_kb)
			{
				// As long as Windows reports more than the safety margin
				// (used to try and prevent crashes), let's average the two
				// values (using a weighting coefficient); we do this because
				// Windows does not report immediately a lowering of allocated
				// memory as an increasing of the available free virtual space,
				// which causes issues with texture bias never lowering while
				// there's free room in the heap again).
				U32 weight = (1024 * (avail_virtual_mem_kb -
									  safety_margin_kb)) /
							 avail_virtual_mem_kb;
				virtual_retained_kb = (avail_virtual_mem_kb * (1024 - weight) +
									  virtual_computed_kb * weight) / 1024;
			}
#  else
			if (avail_virtual_mem_kb > 2 * safety_margin_kb &&
				virtual_computed_kb > avail_virtual_mem_kb)
			{
				// As long as Windows reports more than the safety margin
				// (used to try and prevent crashes), let's average the two
				// values; we do this because Windows does not report
				// immediately a lowering of allocated memory as an
				// increasing of the available free virtual space, which
				// causes issues with texture bias never lowering while
				// there's free room in the heap again).
				virtual_retained_kb = (virtual_computed_kb +
									   avail_virtual_mem_kb) / 2;
			}
#  endif
		}
		LL_DEBUGS("Memory") << "Memory check:\nAvailable virtual space reported by Windows: "
							<< avail_virtual_mem_kb / 1024
							<< "Mb\nComputed available virtual space: "
							<< virtual_computed_kb / 1024
							<< "Mb\nRetained available virtual space: "
							<< virtual_retained_kb / 1024 << "Mb"
							<< LL_ENDL;
		avail_virtual_mem_kb = virtual_retained_kb;
	}
	else
	{
		llwarns << "GetProcessMemoryInfo failed !" << llendl;
	}
# endif

#elif LL_LINUX || LL_DARWIN

	U32 virtual_size_kb = LLOSInfo::getProcessVirtualSizeKB();
	virtual_size_kb += virtual_size_kb / 5;	// 20% penalty for fragmentation
	if (virtual_size_kb < max_virtual_mem_kb)
	{
		avail_virtual_mem_kb = max_virtual_mem_kb - virtual_size_kb;
	}
	else
	{
		avail_virtual_mem_kb = 0;
	}
	LL_DEBUGS("Memory") << "Memory check: Retained available virtual space: "
						<< avail_virtual_mem_kb / 1024 << "Mb" << LL_ENDL;
#else

	// Do not know how to collect available memory info for other systems.
	avail_virtual_mem_kb = max_virtual_mem_kb;

#endif
}

//static
std::string LLMemoryInfo::getInfo()
{
	std::ostringstream s;

#if LL_WINDOWS
	MEMORYSTATUSEX state;
	state.dwLength = sizeof(state);
	GlobalMemoryStatusEx(&state);

	s << "Percent Memory use: " << (U32)state.dwMemoryLoad << '%' << std::endl;
	s << "Total Physical KB:  " << (U32)(state.ullTotalPhys / 1024) << std::endl;
	s << "Avail Physical KB:  " << (U32)(state.ullAvailPhys / 1024) << std::endl;
	s << "Total page KB:      " << (U32)(state.ullTotalPageFile / 1024) << std::endl;
	s << "Avail page KB:      " << (U32)(state.ullAvailPageFile / 1024) << std::endl;
	s << "Total Virtual KB:   " << (U32)(state.ullTotalVirtual / 1024) << std::endl;
	s << "Avail Virtual KB:   " << (U32)(state.ullAvailVirtual / 1024) << std::endl;
#elif LL_DARWIN
	uint64_t phys = 0;

	size_t len = sizeof(phys);

	if (sysctlbyname("hw.memsize", &phys, &len, NULL, 0) == 0)
	{
		s << "Total Physical KB:  " << phys / 1024 << std::endl;
	}
	else
	{
		s << "Unable to collect memory information";
	}
#else
	// *NOTE: This works on Linux. What will it do on other systems ?
	LLFILE* meminfo = LLFile::open(MEMINFO_FILE, "rb");
	if (meminfo)
	{
		char line[MAX_STRING];
		memset(line, 0, MAX_STRING);
		while (fgets(line, MAX_STRING, meminfo))
		{
			line[strlen(line) - 1] = ' ';
			s << line;
		}
		LLFile::close(meminfo);
	}
	else
	{
		s << "Unable to collect memory information";
	}
#endif

	return s.str();
}
