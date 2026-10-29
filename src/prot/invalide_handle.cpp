#include "protection.hpp"

BOOL CloseHandle_InvalideHandle()
{
	// Let's try first with user mode API: CloseHandle
	__try {
		CloseHandle(reinterpret_cast<HANDLE>(0x99999999ULL));
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		return TRUE;
	}
	return FALSE;
}

void c_protection::invalid_handle()
{
	if (CloseHandle_InvalideHandle())
	{
		protection_exit_detection("");
	}
}