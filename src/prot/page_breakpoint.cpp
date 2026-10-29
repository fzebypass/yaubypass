#include "protection.hpp"

std::vector<PVOID> executablePages = {};

BOOL page_breakpoint_ex()
{
	SYSTEM_INFO sysInfo;
	GetSystemInfo(&sysInfo);
	size_t pageSize = sysInfo.dwPageSize;

	HMODULE hMainModule;
	MODULEINFO moduleInfo;

	MEMORY_BASIC_INFORMATION memInfo = { 0 };

	wchar_t buffer[512];

	if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCWSTR)page_breakpoint_ex, &hMainModule))
	{
		if (GetModuleInformation(GetCurrentProcess(), hMainModule, &moduleInfo, sizeof(MODULEINFO)))
		{
			unsigned char* module = static_cast<unsigned char*>(moduleInfo.lpBaseOfDll);
			for (size_t ofs = 0; ofs < moduleInfo.SizeOfImage; ofs += pageSize)
			{
				SecureZeroMemory(buffer, 512);

				if (VirtualQuery(module + ofs, &memInfo, sizeof(MEMORY_BASIC_INFORMATION)) >= sizeof(MEMORY_BASIC_INFORMATION))
				{

					if ((memInfo.Protect & PAGE_EXECUTE) == PAGE_EXECUTE ||
						(memInfo.Protect & PAGE_EXECUTE_READ) == PAGE_EXECUTE_READ ||
						(memInfo.Protect & PAGE_EXECUTE_WRITECOPY) == PAGE_EXECUTE_WRITECOPY ||
						(memInfo.Protect & PAGE_EXECUTE_READWRITE) == PAGE_EXECUTE_READWRITE)
					{
						if ((memInfo.Protect & PAGE_GUARD) == PAGE_GUARD ||
							(memInfo.AllocationProtect & PAGE_GUARD) == PAGE_GUARD)
						{
							return TRUE;
						}
					}

					if ((memInfo.Protect & PAGE_NOACCESS) == PAGE_NOACCESS)
					{
						return TRUE;
					}
				}
			}
		}

		for (PVOID page : executablePages)
		{
			SecureZeroMemory(buffer, 512);

			if (VirtualQuery(page, &memInfo, sizeof(MEMORY_BASIC_INFORMATION)) >= sizeof(MEMORY_BASIC_INFORMATION))
			{

				if (!((memInfo.Protect & PAGE_EXECUTE) == PAGE_EXECUTE ||
					(memInfo.Protect & PAGE_EXECUTE_READ) == PAGE_EXECUTE_READ ||
					(memInfo.Protect & PAGE_EXECUTE_WRITECOPY) == PAGE_EXECUTE_WRITECOPY ||
					(memInfo.Protect & PAGE_EXECUTE_READWRITE) == PAGE_EXECUTE_READWRITE))
				{
					return TRUE;
				}
			}
		}
	}

	return FALSE;
}

__forceinline void init()
{
	SYSTEM_INFO sysInfo;
	GetSystemInfo(&sysInfo);
	size_t pageSize = sysInfo.dwPageSize;

	HMODULE hMainModule;
	MODULEINFO moduleInfo;

	MEMORY_BASIC_INFORMATION memInfo = { 0 };

	if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCWSTR)page_breakpoint_ex, &hMainModule))
	{
		if (GetModuleInformation(GetCurrentProcess(), hMainModule, &moduleInfo, sizeof(MODULEINFO)))
		{
			unsigned char* module = static_cast<unsigned char*>(moduleInfo.lpBaseOfDll);
			for (size_t ofs = 0; ofs < moduleInfo.SizeOfImage; ofs += pageSize)
			{
				if (VirtualQuery(module + ofs, &memInfo, sizeof(MEMORY_BASIC_INFORMATION)) >= sizeof(MEMORY_BASIC_INFORMATION))
				{
					if ((memInfo.Protect & PAGE_EXECUTE) == PAGE_EXECUTE ||
						(memInfo.Protect & PAGE_EXECUTE_READ) == PAGE_EXECUTE_READ ||
						(memInfo.Protect & PAGE_EXECUTE_WRITECOPY) == PAGE_EXECUTE_WRITECOPY ||
						(memInfo.Protect & PAGE_EXECUTE_READWRITE) == PAGE_EXECUTE_READWRITE)
					{
						executablePages.push_back(module + ofs);
					}
				}
			}
		}
	}
}

void c_protection::page_breakpoint()
{
	init();
	if (page_breakpoint_ex())
	{
		protection_exit_detection("");
	}
}