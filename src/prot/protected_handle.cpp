#include "protection.hpp"

BOOL SetHandleInformatiom_ProtectedHandle()
{
	HANDLE hMutex;

	hMutex = CreateMutex(NULL, FALSE, WRAPPER_ALIAN("Random name"));

	if (hMutex) {

		SetHandleInformation(hMutex, HANDLE_FLAG_PROTECT_FROM_CLOSE, HANDLE_FLAG_PROTECT_FROM_CLOSE);

		__try {
			CloseHandle(hMutex);
		}

		__except (EXCEPTION_EXECUTE_HANDLER) {
			return TRUE;
		}

	}

	return FALSE;
}

void c_protection::protected_handle()
{
	if (SetHandleInformatiom_ProtectedHandle())
	{
		protection_exit_detection("");
	}
}