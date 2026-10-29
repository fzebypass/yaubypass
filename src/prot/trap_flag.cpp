#include "protection.hpp"

static BOOL SwallowedException = TRUE;

static LONG CALLBACK VectoredHandler(
	_In_ PEXCEPTION_POINTERS ExceptionInfo
)
{
	SwallowedException = FALSE;

	if (ExceptionInfo->ExceptionRecord->ExceptionCode == EXCEPTION_SINGLE_STEP)
		return EXCEPTION_CONTINUE_EXECUTION;

	return EXCEPTION_CONTINUE_SEARCH;
}

BOOL TrapFlag()
{
	PVOID Handle = AddVectoredExceptionHandler(1, VectoredHandler);
	SwallowedException = TRUE;

	UINT64 eflags = __readeflags();

	eflags |= 0x100;
	__writeeflags(eflags);

	RemoveVectoredExceptionHandler(Handle);
	return SwallowedException;
}

void c_protection::trap_flag()
{
	if (TrapFlag())
	{
		protection_exit_detection("");
	}
}