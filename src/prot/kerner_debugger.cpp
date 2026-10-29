#include "protection.hpp"

BOOL SharedUserData_KernelDebugger()
{
	const ULONG_PTR UserSharedData = 0x7FFE0000;

	const UCHAR KdDebuggerEnabledByte = *(UCHAR*)(UserSharedData + 0x2D4); // 0x2D4 = the offset of the field

	const BOOLEAN KdDebuggerEnabled = (KdDebuggerEnabledByte & 0x1) == 0x1;
	const BOOLEAN KdDebuggerNotPresent = (KdDebuggerEnabledByte & 0x2) == 0;

	if (KdDebuggerEnabled || !KdDebuggerNotPresent)
		return TRUE;

	return FALSE;
}

void c_protection::kernel_debugger()
{
	if (SharedUserData_KernelDebugger())
	{
		protection_exit_detection("");
	}
}