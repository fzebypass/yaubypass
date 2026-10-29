#include "Memory.hpp"
#include <filesystem>
#include <winternl.h>
#include <lazyimporter.hpp>
#include <Globals.hpp>
#include "SDK.hpp"

DWORD WaitForThreadAndReturn(HANDLE hThread) {
	DWORD exitCode = 0;
	if (WaitForSingleObject(hThread, INFINITE) == WAIT_OBJECT_0)
	{
		GetExitCodeThread(hThread, &exitCode);
	}
	CloseHandle(hThread);
	return exitCode;
}

typedef NTSTATUS(WINAPI* pNtCreateThreadEx)(
	OUT PHANDLE hThread,
	IN ACCESS_MASK DesiredAccess,
	IN PVOID ObjectAttributes,
	IN HANDLE ProcessHandle,
	IN PVOID StartRoutine,
	IN PVOID Argument,
	IN ULONG CreateFlags,
	IN SIZE_T ZeroBits,
	IN SIZE_T StackSize,
	IN SIZE_T MaximumStackSize,
	IN PVOID AttributeList
	);
namespace Core {


	BOOL MemoryClass::OpenProc(const char* ProcName) {
		DWORD Pid = GetPidByName(ProcName);
		return OpenProcByPid2(Pid);
	}

	uintptr_t MemoryClass::PatternScan(std::vector<uint8_t> Pattern, int InstructionLength)
	{
		uintptr_t Address = FindSignature(Pattern);

		if (InstructionLength != 0)
		{
			Address = ResolveRelativeAddress(Address, InstructionLength);
		}

		return Address;
	}

	std::string MemoryClass::ReadWString(uintptr_t Addr) {
		const int maxChars = 128;
		wchar_t wbuffer[maxChars];
		int charsRead = 0;


		memset(wbuffer, 0, sizeof(wbuffer));


		while (charsRead < maxChars - 1) {
			wchar_t wchar;
			SIZE_T actualBytesRead = 0;

			if (!ZwReadVirtualMemory(Mem.ProcHandle, (LPVOID)(Addr + charsRead * sizeof(wchar_t)), &wchar, sizeof(wchar_t), &actualBytesRead)) {
				break;
			}

			if (actualBytesRead != sizeof(wchar_t)) {
				break;
			}

			wbuffer[charsRead] = wchar;

			if (wchar == L'\0') {
				break;
			}

			charsRead++;
		}

		wbuffer[charsRead] = L'\0';

		if (charsRead == 0) {
			return "";
		}

		int utf8Size = WideCharToMultiByte(CP_UTF8, 0, wbuffer, charsRead, NULL, 0, NULL, NULL);
		if (utf8Size <= 0) {
			return "";
		}

		std::string result(utf8Size, '\0');
		int actualSize = WideCharToMultiByte(CP_UTF8, 0, wbuffer, charsRead, &result[0], utf8Size, NULL, NULL);

		if (actualSize <= 0) {
			return "";
		}

		result.resize(actualSize);
		return result;
	}
	std::string MemoryClass::ReadWString2(uintptr_t Addr)
	{
		const int maxChars = 128;
		wchar_t wbuffer[maxChars]{};

		int charsRead = 0;

		while (charsRead < maxChars - 1)
		{
			wchar_t wchar = 0;
			SIZE_T actualBytesRead = 0;

			if (!ZwReadVirtualMemory(
				Mem.ProcHandle,
				(LPVOID)(Addr + charsRead * sizeof(wchar_t)),
				&wchar,
				sizeof(wchar_t),
				&actualBytesRead))
			{
				break;
			}

			if (actualBytesRead != sizeof(wchar_t))
				break;

			wbuffer[charsRead] = wchar;

			if (wchar == L'\0')
				break;

			charsRead++;
		}

		if (charsRead <= 0)
			return "";

		int utf8Size = WideCharToMultiByte(
			CP_UTF8, 0,
			wbuffer, charsRead,
			nullptr, 0, nullptr, nullptr
		);

		if (utf8Size <= 0)
			return "";

		std::string result(utf8Size, '\0');

		WideCharToMultiByte(
			CP_UTF8, 0,
			wbuffer, charsRead,
			result.data(), utf8Size,
			nullptr, nullptr
		);

		return result;
	}
	uintptr_t MemoryClass::FindSignature(std::vector<uint8_t> Signature, uintptr_t ModuleBase, uintptr_t ModuleBaseSize) {

		const size_t blockSize = 65536;
		std::vector<uint8_t> data(blockSize);

		size_t sigSize = Signature.size();
		if (sigSize == 0 || sigSize > blockSize)
			return 0;

		uintptr_t Base = ModuleBase ? ModuleBase : ModBase;
		uintptr_t Size = ModuleBaseSize ? ModuleBaseSize : ModBaseSize;

		if (!Base || !Size)
			return 0;

		for (uintptr_t address = Base; address < Base + Size; address += blockSize) {
			SIZE_T bytesRead;
			if (!ReadProcessMemory(ProcHandle, (LPCVOID)address, data.data(), blockSize, &bytesRead))
				continue;

			for (uintptr_t i = 0; i < bytesRead; i++) {
				if (i + sigSize > bytesRead)
					break;

				bool found = true;
				for (uintptr_t j = 0; j < sigSize; j++) {
					if (Signature[j] != 0x00 && data[i + j] != Signature[j]) {
						found = false;
						break;
					}
				}

				if (found)
					return address + i;
			}
		}

		return 0;
	}
	uint64_t MemoryClass::CreateCodeCaveNearby(uintptr_t TargetAddr, size_t Size) {
		const uintptr_t AllocationGranularity = 64 * 1024; // 64KB
		const uintptr_t MaxDist = 0x7FFFFFFF; // 2GB

		// Try allocating forward
		for (uintptr_t offset = 0; offset < MaxDist; offset += AllocationGranularity) {
			uintptr_t addr = (TargetAddr + offset) & ~(AllocationGranularity - 1);
			LPVOID allocated = VirtualAllocEx(ProcHandle, (LPVOID)addr, Size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
			if (allocated) return (uint64_t)allocated;
		}

		// Try allocating backward
		for (uintptr_t offset = AllocationGranularity; offset < MaxDist; offset += AllocationGranularity) {
			uintptr_t addr = (TargetAddr - offset) & ~(AllocationGranularity - 1);
			if (addr < 0x10000) break; // Don't go too low
			LPVOID allocated = VirtualAllocEx(ProcHandle, (LPVOID)addr, Size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
			if (allocated) return (uint64_t)allocated;
		}

		return 0;
	}
	uintptr_t MemoryClass::FindSignatureBypass(std::vector<uint8_t> Signature, uintptr_t ModuleBase, uintptr_t ModuleBaseSize)
	{
		const size_t blockSize = (4096 * 4);
		std::unique_ptr<uint8_t[]> data = std::make_unique<uint8_t[]>(blockSize);

		DWORD oldProtect;
		size_t signatureSize = Signature.size();

		uintptr_t ModulBase;
		uintptr_t ModulBaseSize;

		if (ModuleBase != 0 && ModuleBaseSize != 0) {
			ModulBase = ModuleBase;
			ModulBaseSize = ModuleBaseSize;
		}
		else {
			ModulBase = ModBase;
			ModulBaseSize = ModBaseSize;
		}

		for (uintptr_t address = ModulBase; address < ModulBase + ModulBaseSize; address += blockSize)
		{

			SIZE_T bytesRead;
			if (!ReadProcessMemory(ProcHandle, (void*)address, data.get(), blockSize, &bytesRead)) {
				continue;
			}


			for (uintptr_t i = 0; i < bytesRead; i++)
			{
				for (uintptr_t j = 0; j < signatureSize; j++)
				{
					if (Signature[j] == 0x00)
						continue;

					if (data[i + j] != Signature[j])
						break;

					if (j == signatureSize - 1)
						return (address + i);
				}
			}
		}

		return 0x0;
	}

	uintptr_t MemoryClass::FindSignatureStr(std::string Pattern, uintptr_t ModuleBase, uintptr_t ModuleBaseSize) {
		std::vector<uint8_t> signature = Pattern2Vector(Pattern);
		if (ModuleBase != 0 && ModuleBaseSize != 0) {
			return FindSignature(signature, ModuleBase, ModuleBaseSize);
		}
		else {
			return FindSignature(signature);
		}
	}
	std::vector<uint8_t> MemoryClass::ReadBytes(uintptr_t Addr, size_t Size) {
		std::vector<uint8_t> bytes(Size);
		size_t bytesRead = 0;
		ZwReadVirtualMemory(ProcHandle, (LPCVOID)Addr, bytes.data(), Size, &bytesRead);
		bytes.resize(bytesRead);
		return bytes;
	}

	BOOL MemoryClass::WriteBytes(uintptr_t Addr, std::vector<uint8_t> Bytes) {

		DWORD oldProtect = 0;

		if (!VirtualProtectEx(ProcHandle, (LPVOID)Addr, Bytes.size(), PAGE_EXECUTE_READWRITE, &oldProtect))
			return false;
		NTSTATUS status = ZwWriteVirtualMemory(ProcHandle, (LPVOID)Addr, Bytes.data(), Bytes.size(), NULL);
		DWORD temp = 0;
		VirtualProtectEx(ProcHandle, (LPVOID)Addr, Bytes.size(), oldProtect, &temp);

		return NT_SUCCESS(status);
	}

	BOOL MemoryClass::PatchFunc(uintptr_t Addr, int NopCount)
	{
		if (!NopCount) return false;

		std::vector<uint8_t> PatchTable = { };
		for (int i = 0; i < NopCount; i++)
		{
			PatchTable.push_back({ 0x90 });
		}

		return WriteBytes(Addr, PatchTable);
	}


	BOOL MemoryClass::GetMaxPrivileges(HANDLE hProc) {
		HANDLE h_Token;
		DWORD dw_TokenLength;
		if (OpenProcessToken(hProc, TOKEN_READ | TOKEN_QUERY | TOKEN_ADJUST_PRIVILEGES, &h_Token))
		{
			TOKEN_PRIVILEGES* privilages = new TOKEN_PRIVILEGES[100];
			if (GetTokenInformation(h_Token, TokenPrivileges, privilages, sizeof(TOKEN_PRIVILEGES) * 100, &dw_TokenLength))
			{
				for (int i = 0; i < privilages->PrivilegeCount; i++)
				{
					privilages->Privileges[i].Attributes = SE_PRIVILEGE_ENABLED;
				}
				if (AdjustTokenPrivileges(h_Token, false, privilages, sizeof(TOKEN_PRIVILEGES) * 100, NULL, NULL))
				{
					delete[] privilages;
					return true;
				}
			}
			delete[] privilages;
		}

		return false;
	}

	std::string MemoryClass::ReadString(uintptr_t Addr) {
		const int bufferSize = 256;
		char buffer[bufferSize]{};

		for (int bytesRead = 0; bytesRead < bufferSize - 1; ++bytesRead) {
			char character = '\0';
			SIZE_T actualBytesRead = 0;
			const NTSTATUS status = ZwReadVirtualMemory(
				ProcHandle,
				(LPVOID)(Addr + bytesRead),
				&character,
				sizeof(character),
				&actualBytesRead
			);

			if (!NT_SUCCESS(status) || actualBytesRead != sizeof(character)) {
				break;
			}

			buffer[bytesRead] = character;

			if (character == '\0') {
				return std::string(buffer);
			}
		}

		return buffer[0] != '\0' ? std::string(buffer) : "";
	}

	DWORD MemoryClass::GetPidByName(const char* ProcName) {
		HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
		if (hSnapshot == INVALID_HANDLE_VALUE) {
			return 0;
		}

		PROCESSENTRY32 pe32;
		pe32.dwSize = sizeof(PROCESSENTRY32);

		if (!Process32First(hSnapshot, &pe32)) {
			CloseHandle(hSnapshot);
			return 0;
		}

		DWORD pid = 0;
		do {
			if (_stricmp(pe32.szExeFile, ProcName) == 0) {
				pid = pe32.th32ProcessID;
				break;
			}
		} while (Process32Next(hSnapshot, &pe32));

		CloseHandle(hSnapshot);
		return pid;
	}

	std::string MemoryClass::GetNameByPid(DWORD Pid) {
		HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, Pid);
		if (!hProcess) { return ""; }

		char processName[MAX_PATH];
		if (!GetModuleFileNameEx(hProcess, NULL, processName, MAX_PATH)) { return ""; }

		CloseHandle(hProcess);

		std::string fullPath(processName);
		size_t pos = fullPath.find_last_of(xorstr("\\/"));
		if (pos != std::string::npos) {
			return fullPath.substr(pos + 1);
		}
		else {
			return fullPath;
		}
	}

	uintptr_t MemoryClass::GetModuleBaseAddr(DWORD ProcId, const char* ModuleName, uintptr_t* ModSize) {
		HANDLE hSnapShot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, ProcId);

		if (hSnapShot == INVALID_HANDLE_VALUE) { return 0; }

		MODULEENTRY32 ModuleEntry;
		ModuleEntry.dwSize = sizeof(MODULEENTRY32);
		if (Module32First(hSnapShot, &ModuleEntry)) {
			do {
				if (strcmp(ModuleEntry.szModule, ModuleName) == 0) {
					break;
				}
			} while (Module32Next(hSnapShot, &ModuleEntry));
		}
		CloseHandle(hSnapShot);

		*ModSize = (uintptr_t)ModuleEntry.modBaseSize;
		return (uintptr_t)ModuleEntry.modBaseAddr;
	}

	DWORD MemoryClass::execShellcode(uintptr_t codeCaveAddress, const unsigned char* shellcode, size_t shellcodeSize) {
		SIZE_T bytesWritten = 0;

		if (!codeCaveAddress || !shellcode || shellcodeSize == 0)
			return false;

		if (!WriteProcessMemory(ProcHandle, reinterpret_cast<LPVOID>(codeCaveAddress), shellcode, shellcodeSize, &bytesWritten))
			return false;


		static HMODULE hNtdll = GetModuleHandleA(xorstr("ntdll.dll"));

		if (!hNtdll)
			return false;

		pNtCreateThreadEx pfnNtCreateThreadEx = (pNtCreateThreadEx)GetProcAddress(hNtdll, xorstr("NtCreateThreadEx"));
		if (!pfnNtCreateThreadEx)
			return 1;

		HANDLE hRemoteThread = nullptr;
		NTSTATUS status = pfnNtCreateThreadEx(&hRemoteThread, THREAD_ALL_ACCESS, nullptr, ProcHandle, reinterpret_cast<LPTHREAD_START_ROUTINE>(codeCaveAddress), nullptr, FALSE, 0, 0, 0, nullptr);

		if (!hRemoteThread)
			return false;

		return WaitForThreadAndReturn(hRemoteThread);
	}

	/*uint64_t MemoryClass::GetModuleBaseByName(DWORD Pid, std::wstring ModuleName)
	{
		HANDLE hSnapshot = SafeCall(CreateToolhelp32Snapshot)(TH32CS_SNAPMODULE, Pid);
		if (!hSnapshot || hSnapshot == INVALID_HANDLE_VALUE || hSnapshot == ((HANDLE)(LONG_PTR)ERROR_BAD_LENGTH))
		{
			return 0;
		}

		uint64_t ModuleBase;
		MODULEENTRY32 ModuleEntry;
		ModuleEntry.dwSize = sizeof(ModuleEntry);
		if (SafeCall(Module32First)(hSnapshot, &ModuleEntry))
		{
			while (_wcsicmp(ModuleEntry.szModule, ModuleName.c_str()))
			{
				if (!SafeCall(Module32Next)(hSnapshot, &ModuleEntry))
				{
					SafeCall(CloseHandle)(hSnapshot);
					return 0;
				}
			}

			ModuleBase = (uint64_t)ModuleEntry.modBaseAddr;
		}
		else
		{
			SafeCall(CloseHandle)(hSnapshot);
			return 0;
		}

		SafeCall(CloseHandle)(hSnapshot);
		return ModuleBase;
	}

	uint64_t MemoryClass::GetModuleBaseSizeByName(DWORD Pid, std::wstring ModuleName)
	{
		HANDLE hSnapshot = SafeCall(CreateToolhelp32Snapshot)(TH32CS_SNAPMODULE, Pid);
		if (!hSnapshot || hSnapshot == INVALID_HANDLE_VALUE || hSnapshot == ((HANDLE)(LONG_PTR)ERROR_BAD_LENGTH))
		{
			return 0;
		}

		uint64_t ModuleBase;
		MODULEENTRY32 ModuleEntry;
		ModuleEntry.dwSize = sizeof(ModuleEntry);
		if (SafeCall(Module32First)(hSnapshot, &ModuleEntry))
		{
			while (_wcsicmp(ModuleEntry.szModule, ModuleName.c_str()))
			{
				if (!SafeCall(Module32Next)(hSnapshot, &ModuleEntry))
				{
					SafeCall(CloseHandle)(hSnapshot);
					return 0;
				}
			}

			ModuleBase = (uint64_t)ModuleEntry.modBaseSize;
		}
		else
		{
			SafeCall(CloseHandle)(hSnapshot);
			return 0;
		}

		SafeCall(CloseHandle)(hSnapshot);

		return ModuleBase;
	}*/


	std::vector<DWORD> MemoryClass::GetProcessesByName(const std::wstring& process_name)
	{
		std::vector<DWORD> process_ids;
		HANDLE h_process_snap;
		PROCESSENTRY32W pe32;
		pe32.dwSize = sizeof(PROCESSENTRY32W);

		h_process_snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
		if (h_process_snap == INVALID_HANDLE_VALUE)
			return process_ids;

		if (!Process32FirstW(h_process_snap, &pe32))
		{
			CloseHandle(h_process_snap);
			return process_ids;
		}

		do
		{
			if (_wcsicmp(pe32.szExeFile, process_name.c_str()) == 0)
				process_ids.push_back(pe32.th32ProcessID);
		} while (Process32NextW(h_process_snap, &pe32));

		CloseHandle(h_process_snap);
		return process_ids;
	}

	uintptr_t MemoryClass::AllocateExecutableMemory(size_t size)
	{
		return (uintptr_t)VirtualAllocEx(ProcHandle, nullptr, size,
			MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
	}

	DWORD MemoryClass::ExecuteShellcode(const unsigned char* shellcode, size_t shellcodeSize)
	{
		if (!shellcode || shellcodeSize == 0 || !ProcHandle)
			return 0;

		uintptr_t pRemote = AllocateExecutableMemory(shellcodeSize);
		if (!pRemote)
			return 0;

		SIZE_T written = 0;
		if (!WriteProcessMemory(ProcHandle, (LPVOID)pRemote, shellcode, shellcodeSize, &written) ||
			written != shellcodeSize)
		{
			VirtualFreeEx(ProcHandle, (LPVOID)pRemote, 0, MEM_RELEASE);
			return 0;
		}

		HANDLE hRemoteThread = nullptr;
		static pNtCreateThreadEx NtCreateThreadEx = nullptr;

		if (!NtCreateThreadEx)
		{
			HMODULE hNtdll = GetModuleHandleA("ntdll.dll");
			if (hNtdll)
				NtCreateThreadEx = (pNtCreateThreadEx)GetProcAddress(hNtdll, "NtCreateThreadEx");
		}

		if (!NtCreateThreadEx)
			return 0;

		NTSTATUS status = NtCreateThreadEx(
			&hRemoteThread,
			THREAD_ALL_ACCESS,
			nullptr,
			ProcHandle,
			(LPTHREAD_START_ROUTINE)pRemote,
			nullptr,
			FALSE,
			0, 0, 0,
			nullptr
		);

		if (!NT_SUCCESS(status) || !hRemoteThread)
		{
			VirtualFreeEx(ProcHandle, (LPVOID)pRemote, 0, MEM_RELEASE);
			return 0;
		}

		DWORD exitCode = WaitForThreadAndReturn(hRemoteThread);

		VirtualFreeEx(ProcHandle, (LPVOID)pRemote, 0, MEM_RELEASE);

		return exitCode;
	}
	bool MemoryClass::IsModuleLoaded(DWORD process_id, const std::wstring& module_name)
	{
		HANDLE h_module_snap = INVALID_HANDLE_VALUE;
		MODULEENTRY32W me32;
		me32.dwSize = sizeof(MODULEENTRY32W);

		h_module_snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, process_id);
		if (h_module_snap == INVALID_HANDLE_VALUE)
			return false;

		if (!Module32FirstW(h_module_snap, &me32))
		{
			CloseHandle(h_module_snap);
			return false;
		}

		do
		{
			if (_wcsicmp(me32.szModule, module_name.c_str()) == 0 ||
				_wcsicmp(me32.szExePath, module_name.c_str()) == 0)
			{
				CloseHandle(h_module_snap);
				return true;
			}
		} while (Module32NextW(h_module_snap, &me32));

		CloseHandle(h_module_snap);
		return false;
	}
	uintptr_t MemoryClass::ScanOffset(const char* sig, bool rip, bool deref)
	{
		uintptr_t found = FindSignatureStr(sig); // sempre scan REAL

		if (!found)
			return 0;

		uintptr_t resolved = found;

		if (rip)
			resolved = resolveRelativeAddress(found, 3, 7);

		if (deref)
			resolved = Read<uintptr_t>(resolved);

		return resolved - ModBase;
	}
	uintptr_t MemoryClass::resolveRelativeAddress(uintptr_t instr, int offsetOffset, int instrSize)
	{
		int32_t ripOffset = Mem.Read<int32_t>(instr + offsetOffset);
		return instr + instrSize + ripOffset;
	}
	uintptr_t MemoryClass::GetRemoteModuleBaseAddress(void* h_process, const wchar_t* module_name)
	{
		HANDLE h_snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, GetProcessId((HANDLE)h_process));
		if (h_snapshot == INVALID_HANDLE_VALUE) return 0;

		MODULEENTRY32W me;
		me.dwSize = sizeof(MODULEENTRY32W);
		uintptr_t module_base = 0;
		if (Module32FirstW(h_snapshot, &me))
		{
			do
			{
				if (_wcsicmp(me.szModule, module_name) == 0) {
					module_base = (uintptr_t)me.modBaseAddr;
					break;
				}
			} while (Module32NextW(h_snapshot, &me));
		}
		CloseHandle(h_snapshot);
		return module_base;
	}

	uintptr_t MemoryClass::GetExportedFunctionAddress(void* h_process, uintptr_t module_base, const wchar_t* module_name, const char* function_name) {
		HMODULE h_local_module = LoadLibraryW(module_name);
		if (!h_local_module) return 0;

		FARPROC local_proc_address = GetProcAddress(h_local_module, function_name);
		if (!local_proc_address)
		{
			FreeLibrary(h_local_module);
			return 0;
		}

		uintptr_t offset = (uintptr_t)local_proc_address - (uintptr_t)h_local_module;
		FreeLibrary(h_local_module);
		return module_base + offset;
	}

	uintptr_t MemoryClass::AllocateMemoryNearAddress(void* process, uintptr_t desired_address, SIZE_T size, DWORD protection, SIZE_T range) {
		const SIZE_T step = 0x1000;
		uintptr_t base_address = desired_address - range;
		uintptr_t end_address = desired_address + range;

		for (uintptr_t address = base_address; address < end_address; address += step)
		{
			void* allocated_memory = VirtualAllocEx(
				(HANDLE)process,
				reinterpret_cast<void*>(address),
				size,
				MEM_RESERVE | MEM_COMMIT,
				protection
			);

			if (allocated_memory != NULL)
				return reinterpret_cast<uintptr_t>(allocated_memory);
		}

		return NULL;
	}

	bool MemoryClass::AssembleJumpNearInstruction(uint8_t* buffer, uintptr_t source_address, uintptr_t target_address) {
		intptr_t jump_offset = target_address - (source_address + 5);

		if (std::abs(jump_offset) > 0x7FFFFFFF)
			return false;

		buffer[0] = 0xE9;
		*reinterpret_cast<int32_t*>(buffer + 1) = static_cast<int32_t>(jump_offset);

		return true;
	}

	bool MemoryClass::WriteMemory(void* h_process, uintptr_t address, const void* buffer, SIZE_T size) {
		SIZE_T written;
		return WriteProcessMemory((HANDLE)h_process, reinterpret_cast<void*>(address), buffer, size, &written) && written == size;
	}

	bool MemoryClass::WriteMemoryWithProtection(void* h_process, uintptr_t address, const void* buffer, SIZE_T size) {
		DWORD old_protect;

		if (!VirtualProtectEx((HANDLE)h_process, reinterpret_cast<void*>(address), size, PAGE_EXECUTE_READWRITE, &old_protect))
			return false;

		bool success = WriteMemory(h_process, address, buffer, size);

		if (!VirtualProtectEx((HANDLE)h_process, reinterpret_cast<void*>(address), size, old_protect, &old_protect))
			return false;

		return success;
	}

	bool MemoryClass::WriteMemoryWithProtectionDynamic(void* h_process, uintptr_t address, const std::vector<uint8_t>& buffer) {
		DWORD old_protect;

		if (!VirtualProtectEx((HANDLE)h_process, reinterpret_cast<void*>(address), buffer.size(), PAGE_EXECUTE_READWRITE, &old_protect))
			return false;

		bool success = WriteMemory(h_process, address, buffer.data(), buffer.size());

		if (!VirtualProtectEx((HANDLE)h_process, reinterpret_cast<void*>(address), buffer.size(), old_protect, &old_protect))
			return false;

		return success;
	}

	bool MemoryClass::IsProcessAlive() const {
		const HANDLE process_handle = ProcHandle;
		if (!process_handle || process_handle == INVALID_HANDLE_VALUE) {
			return false;
		}

		return WaitForSingleObject(process_handle, 0) == WAIT_TIMEOUT;
	}

	void MemoryClass::ResetProcessContext() {
		HANDLE process_handle = ProcHandle;
		HANDLE attached_process_handle = AttachedProcessHandle;

		ProcId = 0;
		ProcHandle = nullptr;
		ProcName.clear();
		ProcName2.clear();
		ModBase = 0;
		ModBaseSize = 0;

		AttachedProcessHandle = nullptr;
		AttachedProcessPid = 0;
		g_Variables.ProcIdFiveM = 0;

		if (attached_process_handle &&
			attached_process_handle != INVALID_HANDLE_VALUE &&
			attached_process_handle != process_handle) {
			CloseHandle(attached_process_handle);
		}

		if (process_handle && process_handle != INVALID_HANDLE_VALUE) {
			CloseHandle(process_handle);
		}
	}

	BOOL MemoryClass::OpenProcByPid() {
		if (g_Variables.ProcIdFiveM == 0) { return false; }

		const DWORD process_id = g_Variables.ProcIdFiveM;
		const std::wstring process_name = ProcName;
		ResetProcessContext();
		g_Variables.ProcIdFiveM = process_id;
		ProcName = process_name;
		ProcHandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, process_id);
		if (!ProcHandle) { return false; };

		/*Mem.ModBase = Mem.GetModuleBaseByName(g_Variables.ProcIdFiveM, Mem.ProcName);
		Mem.ModBaseSize = Mem.GetModuleBaseSizeByName(g_Variables.ProcIdFiveM, Mem.ProcName);*/
		std::string procNameStr(ProcName.begin(), ProcName.end());
		ProcId = process_id;
		ModBase = GetModuleBaseAddr(process_id, procNameStr.c_str(), &ModBaseSize);


		return true;
	}

	BOOL MemoryClass::OpenProcByPid2(DWORD Pid) {
		if (Pid == 0) { return false; }

		ResetProcessContext();
		g_Variables.ProcIdFiveM = Pid;
		ProcId = Pid;
		ProcHandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, ProcId);
		if (!ProcHandle) { return false;; };

		ProcName2 = GetNameByPid(ProcId);
		ModBase = GetModuleBaseAddr(ProcId, ProcName2.c_str(), &ModBaseSize);
		return true;
	}

	bool MemoryClass::WriteProcessMemoryImpl(uint64_t WriteAddress, LPVOID Value, SIZE_T Size)
	{
		if (ProcHandle && ProcId && WriteProcessMemory(ProcHandle, (LPVOID)WriteAddress, Value, Size, nullptr))
		{
			return true;
		}

		return false;
	}

	void MemoryClass::AttachProces(DWORD Pid)
	{
		ResetProcessContext();
		g_Variables.ProcIdFiveM = Pid;
		AttachedProcessHandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, Pid);
		AttachedProcessPid = Pid;


		this->ProcHandle = AttachedProcessHandle;
		this->ProcId = Pid;
		this->ProcName2 = GetNameByPid(Pid);
		this->ModBase = GetModuleBaseAddr(Pid, this->ProcName2.c_str(), &this->ModBaseSize);
	}
	void MemoryClass::ReadChunkToBuffer(const std::uintptr_t& address, char* buffer, std::size_t size) const noexcept {
		SIZE_T bytesRead = 0;
		ZwReadVirtualMemory(Mem.ProcHandle, (PVOID)(address), (PVOID)(buffer), size, &bytesRead);
	}
	class CRapidFire
	{
		uintptr_t pFunc;
		uintptr_t pShellcode;

		BYTE jump[14] = { 0xFF, 0x25,0,0,0,0, 0,0,0,0,0,0,0,0 };
		BYTE orig[sizeof(jump)];

		size_t sizeOfShellcode;

		bool bInitialized = false;
		bool bEnabled = false;

	public:
		CRapidFire() : pFunc(0), pShellcode(0), sizeOfShellcode(0), bInitialized(false), bEnabled(false) {}
		~CRapidFire();

		void Initialize();
		void Enable(bool enable);
		void SetValue(float value);
		void SetLocalPlayer(uintptr_t LocalPlayer);
		bool IsEnabled() const { return bEnabled; }
		bool IsInitialized() const { return bInitialized; }
	};

	static CRapidFire g_RapidFire;

	void CRapidFire::Initialize()
	{
		if (bInitialized) return;

		pFunc = Mem.FindSignatureStr("48 8B 5C 24 50 0F 28 74 24 30 0F 28 C7 0F 28 7C 24 20 48 83 C4 40 5F C3 90");
		if (!pFunc)
		{
			pFunc = Mem.FindSignatureStr("48 8B 5C 24 50 0F 28 74 24 30 0F 28 C7 0F 28 7C 24 20 48 83 C4 40 5F C3 CC");
		}

		if (!pFunc) return;

		pFunc += 10;

		BYTE shellcode[] =
		{
			0x0F, 0x28, 0xC7,
			0x0F, 0x28, 0x7C, 0x24, 0x20,

			0x49, 0xBB, 0,0,0,0,0,0,0,0, // + 10 local player goes here
			0x4C, 0x39, 0xDF,
			0x75, 0x0D,

			0xB8, 0,0,0,0, // + 24 
			0x66, 0x0F, 0x6E, 0xC8,
			0xF3, 0x0F, 0x59, 0xC1,

			0x48, 0x83, 0xC4, 0x40,
			0x5F,
			0xC3
		};

		sizeOfShellcode = sizeof(shellcode);

		pShellcode = reinterpret_cast<uintptr_t>(VirtualAllocEx(Mem.ProcHandle, nullptr, sizeOfShellcode, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
		if (!pShellcode) return;

		*reinterpret_cast<float*>(shellcode + 24) = 1.f;

		ReadProcessMemory(Mem.ProcHandle, reinterpret_cast<void*>(pFunc), &orig, sizeof(orig), nullptr);
		WriteProcessMemory(Mem.ProcHandle, reinterpret_cast<void*>(pShellcode), &shellcode, sizeOfShellcode, nullptr);

		*reinterpret_cast<uintptr_t*>(jump + 6) = pShellcode;

		bInitialized = true;
	}

	void CRapidFire::Enable(bool enable)
	{
		if (!bInitialized) return;

		if (enable && !bEnabled)
		{
			bEnabled = WriteProcessMemory(Mem.ProcHandle, reinterpret_cast<void*>(pFunc), &jump, sizeof(jump), nullptr) != FALSE;
		}
		else if (!enable && bEnabled)
		{
			if (WriteProcessMemory(Mem.ProcHandle, reinterpret_cast<void*>(pFunc), &orig, sizeof(orig), nullptr))
				bEnabled = false;
		}
	}

	void CRapidFire::SetValue(float value)
	{
		if (!bInitialized) return;
		WriteProcessMemory(Mem.ProcHandle, reinterpret_cast<void*>(pShellcode + 24), &value, sizeof(value), nullptr);
	}

	void CRapidFire::SetLocalPlayer(uintptr_t LocalPlayer)
	{
		if (!bInitialized) return;

		static uintptr_t last = 0;

		if (last != LocalPlayer)
		{
			WriteProcessMemory(Mem.ProcHandle, reinterpret_cast<void*>(pShellcode + 10), &LocalPlayer, sizeof(LocalPlayer), nullptr);
			last = LocalPlayer;
		}
	}

	CRapidFire::~CRapidFire()
	{
		if (!bInitialized) return;

		Enable(false);
		if (pShellcode)
		{
			VirtualFreeEx(Mem.ProcHandle, reinterpret_cast<void*>(pShellcode), 0, MEM_RELEASE);
		}
	}
	void MemoryClass::RapidFireUpdate(bool force_disable)
	{
		static bool bInitialized = false;
		static bool bLastEnabled = false;
		static uintptr_t lastLocalPlayer = 0;

		if (!bInitialized)
		{
			g_RapidFire.Initialize();
			bInitialized = true;
		}

		bool bCurrentlyEnabled = force_disable ? false : g_Config.Player->RapidFire;

		if (bCurrentlyEnabled != bLastEnabled)
		{
			if (bCurrentlyEnabled)
			{
				g_RapidFire.Enable(true);
				g_RapidFire.SetValue(g_Config.Player->RapidFireSpeed);
			}
			else
			{
				g_RapidFire.Enable(false);
			}
			bLastEnabled = bCurrentlyEnabled;
		}

		if (bCurrentlyEnabled && g_RapidFire.IsInitialized())
		{
			uintptr_t currentLocalPlayer = reinterpret_cast<uintptr_t>(Core::SDK::Pointers::pLocalPlayer);
			if (lastLocalPlayer != currentLocalPlayer)
			{
				g_RapidFire.SetLocalPlayer(currentLocalPlayer);
				lastLocalPlayer = currentLocalPlayer;
			}

			static float lastValue = 0.f;
			if (lastValue != g_Config.Player->RapidFireSpeed)
			{
				g_RapidFire.SetValue(g_Config.Player->RapidFireSpeed);
				lastValue = g_Config.Player->RapidFireSpeed;
			}
		}
		else if (!bCurrentlyEnabled)
		{
			lastLocalPlayer = 0;
		}
	}
}
