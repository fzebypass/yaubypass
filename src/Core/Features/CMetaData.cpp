#include "CMetaData.h"
#include <Core/SDK/Memory.hpp>

bool SDK::CMetaData::bIsFristPerson() {
    return Core::Mem.Read<float>((uintptr_t)this + 0x30) == 0.0f;
}

void SDK::CMetaData::fSetFov(float fValue) {
    Core::Mem.Write<float>((uintptr_t)this + 0x30, fValue);
}

void SDK::CMetaData::fSetAltitude(float fValue) {
    Core::Mem.Write<float>((uintptr_t)this + 0x88, fValue);
}

void SDK::CMetaData::SetVectorFix(bool bIsInVehicle) {
    if (bIsInVehicle) {
        if (Core::Mem.Read<float>((uintptr_t)this + 0x2AC) == -2.0f) {
            Core::Mem.Write<float>((uintptr_t)this + 0x2AC, 0.0f);
            Core::Mem.Write<float>((uintptr_t)this + 0x2C0, 111.0f);
            Core::Mem.Write<float>((uintptr_t)this + 0x2C4, 111.0f);
        }
    }
}