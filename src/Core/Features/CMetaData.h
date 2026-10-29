#pragma once

namespace SDK {
    class CMetaData {
    public:
        bool bIsFristPerson();
        void fSetFov(float fValue);
        void fSetAltitude(float fValue);
        void SetVectorFix(bool bIsInVehicle);
    };
}