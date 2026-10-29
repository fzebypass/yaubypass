#pragma once

#include <cstdint>

// Returns 0 if clean, non-zero mask if VM detected
// FLG_BIOS 0x1 | FLG_CPU 0x2 | FLG_TIME 0x4
unsigned int check_anti_vm();
