#include "protection.hpp"

BOOL bIsBeinDbg = TRUE;

LONG WINAPI UnhandledExcepFilter(PEXCEPTION_POINTERS pExcepPointers)
{
	// If a debugger is present, then this function will not be reached.
	bIsBeinDbg = FALSE;
	return EXCEPTION_CONTINUE_EXECUTION;
}


BOOL UnhandledExcepFilterTest()
{
	LPTOP_LEVEL_EXCEPTION_FILTER Top = SetUnhandledExceptionFilter(UnhandledExcepFilter);
	RaiseException(EXCEPTION_FLT_DIVIDE_BY_ZERO, 0, 0, NULL);
	SetUnhandledExceptionFilter(Top);
	return bIsBeinDbg;
}

void c_protection::unhandle_debug()
{
	if (UnhandledExcepFilterTest())
	{
		protection_exit_detection("");
	}
}