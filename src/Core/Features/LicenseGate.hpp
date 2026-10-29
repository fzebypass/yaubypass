#pragma once
// ---------------------------------------------------------------------------
//  PRUDA licence gate
//  Blocks the cheat until a valid key is entered. Implemented in
//  LicenseGate.cpp (kept out of this header: GDI+'s "byte" clashes with
//  std::byte in every translation unit that pulls in <Includes/Includes.hpp>).
// ---------------------------------------------------------------------------

#include <windows.h>

namespace LicenseGate {
	// Shows the key window. Returns true only when the server accepted the key.
	bool Run(HINSTANCE instance);
}
