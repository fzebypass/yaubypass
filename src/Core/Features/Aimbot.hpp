#pragma once
#define NOMINMAX
#include <Includes/Includes.hpp>
#include <Core/Offsets.hpp>
#include <Core/SDK/SDK.hpp>

namespace Core
{
	namespace Features
	{
		class cAimbot
		{
		private:
			void SetViewAngles(CPed* Ped, D3DXVECTOR3 BonePos);
		public:
			void Start(uint64_t worker_epoch);
		};

		inline cAimbot g_Aimbot;
	}
}
