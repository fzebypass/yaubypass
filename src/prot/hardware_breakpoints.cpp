#include "protection.hpp"

BOOL HardwareBreakpoints()
{
	BOOL bResult = FALSE;

	// This structure is key to the function and is the 
	// medium for detection and removal
	PCONTEXT ctx = PCONTEXT(VirtualAlloc(NULL, sizeof(CONTEXT), MEM_COMMIT, PAGE_READWRITE));

	if (ctx) {

		SecureZeroMemory(ctx, sizeof(CONTEXT));

		// The CONTEXT structure is an in/out parameter therefore we have
		// to set the flags so Get/SetThreadContext knows what to set or get.
		ctx->ContextFlags = CONTEXT_DEBUG_REGISTERS;

		// Get the registers
		if (GetThreadContext(GetCurrentThread(), ctx)) {

			// Now we can check for hardware breakpoints, its not 
			// necessary to check Dr6 and Dr7, however feel free to
			if (ctx->Dr0 != 0 || ctx->Dr1 != 0 || ctx->Dr2 != 0 || ctx->Dr3 != 0)
				bResult = TRUE;
		}

		VirtualFree(ctx, 0, MEM_RELEASE);
	}

	return bResult;
}

void c_protection::hardware_breakpoints()
{
	if (HardwareBreakpoints())
	{
		protection_exit_detection("");
	}
}