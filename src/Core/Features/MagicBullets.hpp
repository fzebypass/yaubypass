#pragma once
#include <Includes/Includes.hpp>
#include <Core/Offsets.hpp>
#include <Core/SDK/SDK.hpp>
#include <core/Features/dynamicfov.hpp>

namespace Core
{
	namespace Features
	{
		class cMagicBullets {
		private:
			bool Initialized;
			uintptr_t GetCWeaponObj();
			uintptr_t CWeapon;
		public:
			void Initialize();
			void Restore();
			void Start();
		};

		inline cMagicBullets g_MagicBullets;

	}
}