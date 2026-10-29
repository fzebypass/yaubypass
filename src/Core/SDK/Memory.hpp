#pragma once
#include <Core/SDK/Structs/Structs.hpp>
#include <Security/xorstr.hpp>
#include <tlhelp32.h>
#include <windows.h>
#include <iostream>
#include <psapi.h>
#include <sstream>
#include <cstring>
#include <vector>
#include <regex>
#pragma comment(lib, "ntdll.lib")

typedef struct _MEMORY_REGION {
	DWORD_PTR dwBaseAddr;
	DWORD_PTR dwMemorySize;
}MEMORY_REGION;

namespace Core {


	class MemoryClass {
	public:
		DWORD ProcId;
		HANDLE ProcHandle;
		std::wstring ProcName;
		std::string ProcName2;
		uintptr_t ModBase, ModBaseSize;

		HANDLE AttachedProcessHandle = nullptr;
		DWORD  AttachedProcessPid = 0;
		std::string ReadWString(uintptr_t Addr);
		std::string ReadWString2(uintptr_t Addr);
		DWORD GetPidByName(const char* ProcName);
		std::string GetNameByPid(DWORD Pid);
		uintptr_t GetModuleBaseAddr(DWORD ProcId, const char* ModuleName, uintptr_t* ModSize);
		uintptr_t CreateCodeCaveNearby(uintptr_t TargetAddr, size_t Size);

		BOOL OpenProc(const char* ProcName);
		BOOL OpenProcByPid2(DWORD Pid);

		BOOL OpenProcByPid();
		BOOL GetMaxPrivileges(HANDLE hProc);

		std::string ReadString(uintptr_t Addr);
		std::vector<uint8_t> ReadBytes(uintptr_t Addr, size_t Size);
		BOOL WriteBytes(uintptr_t Addr, std::vector<uint8_t> Bytes);
		BOOL PatchFunc(uintptr_t Addr, int NopCount);
		DWORD execShellcode(uintptr_t codeCaveAddress, const unsigned char* shellcode, size_t shellcodeSize);

		uintptr_t FindSignature(std::vector<uint8_t> Signature, uintptr_t ModuleBase = 0, uintptr_t ModuleBaseSize = 0);
		uintptr_t FindSignatureStr(std::string Pattern, uintptr_t ModuleBase = 0, uintptr_t ModuleBaseSize = 0);
		uintptr_t PatternScan(std::vector<uint8_t> Pattern, int InstructionLength);
		uintptr_t FindSignatureBypass(std::vector<uint8_t> Signature, uintptr_t ModuleBase, uintptr_t ModuleBaseSize);

		std::vector<DWORD> GetProcessesByName(const std::wstring& process_name);
		bool IsModuleLoaded(DWORD process_id, const std::wstring& module_name);
		uintptr_t GetRemoteModuleBaseAddress(void* h_process, const wchar_t* module_name);
		uintptr_t GetExportedFunctionAddress(void* hProcess, uintptr_t module_base, const wchar_t* module_name, const char* function_name);
		uintptr_t resolveRelativeAddress(uintptr_t instr, int offsetOffset, int instrSize);
		uintptr_t AllocateMemoryNearAddress(void* process, uintptr_t desired_address, SIZE_T size, DWORD protection = PAGE_EXECUTE_READWRITE, SIZE_T range = 0x20000000 - 0x2000);
		DWORD ExecuteShellcode(const unsigned char* shellcode, size_t shellcodeSize);
		uintptr_t AllocateExecutableMemory(size_t size);
		bool WriteMemoryWithProtectionDynamic(void* h_process, uintptr_t address, const std::vector<uint8_t>& buffer);
		bool WriteMemory(void* h_process, uintptr_t address, const void* buffer, SIZE_T size);
		bool WriteMemoryWithProtection(void* h_process, uintptr_t address, const void* buffer, SIZE_T size);
		bool WriteProcessMemoryImpl(uint64_t WriteAddress, LPVOID Value, SIZE_T Size);
		bool AssembleJumpNearInstruction(uint8_t* buffer, uintptr_t source_address, uintptr_t target_address);
		bool IsProcessAlive() const;
		void ResetProcessContext();
		void AttachProces(DWORD Pid);
		void ReadChunkToBuffer(const std::uintptr_t& address, char* buffer, std::size_t size)const noexcept;
		void RapidFireUpdate(bool force_disable = false);
		template <typename T>
		T Read(uintptr_t Addr) {
			T value = { };
			ZwReadVirtualMemory(ProcHandle, (LPCVOID)Addr, &value, sizeof(value), NULL);
			return value;
		}

		template <typename T>
		BOOL Write(uintptr_t Addr, T Value) {
			return ZwWriteVirtualMemory(ProcHandle, (LPVOID)Addr, &Value, sizeof(Value), NULL);
		}

		uintptr_t CreateCodeCave(size_t Size) {
			LPVOID CaveAddress = VirtualAllocEx(ProcHandle, NULL, Size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
			return (uintptr_t)CaveAddress;
		}

		BOOL FreeCave(uintptr_t Addr) {
			return VirtualFreeEx(ProcHandle, (LPVOID)Addr, 0, MEM_RELEASE);
		}

		BOOL HookJMP(uintptr_t HookAddress, uintptr_t JmpToAddress) {
			BYTE jmpPatch[] = { 0xFF, 0x25, 0x00, 0x00, 0x00, 0x00 };
			for (int x = 0; x < sizeof(jmpPatch); x++) {
				Write<BYTE>(HookAddress + x, jmpPatch[x]);
			}

			Write<uintptr_t>(HookAddress + 6, JmpToAddress);

			return true;
		}
		uintptr_t ResolveRelativeAddress(uintptr_t Addr, int InstructionLength)
		{
			uintptr_t Offset = Read<uintptr_t>(Addr + (InstructionLength - 4));
			return Addr + InstructionLength + Offset;
		}

		uintptr_t ScanOffset(const char* sig, bool rip, bool deref);


		std::vector<uint8_t> Pattern2Vector(std::string Pattern) {
			std::vector<uint8_t> result;
			std::stringstream ss(Pattern);
			std::string token;

			while (ss >> token) {
				if (token == xorstr("??")) {
					result.push_back(0x00);
				}
				else if (token == xorstr("?")) {
					result.push_back(0x00);
				}
				else {
					uint8_t value = std::stoul(token, nullptr, 16);
					result.push_back(value);
				}
			}

			return result;
		}


	};

	inline MemoryClass Mem;
}
