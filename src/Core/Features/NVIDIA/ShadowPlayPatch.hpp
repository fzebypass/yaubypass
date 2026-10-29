#pragma once

namespace Core {
	namespace Features {
		namespace ShadowPlayPatch {
			// Streamproof (idempotent on loader re-run):
			// - Affinity/Process32 spoof so Instant Replay stays on
			// - FBC->DDA retarget for overlay exclusion
			// - Skips remap if Local\CapPipe.<pid> already exists
			// Live state of the injection, shown in the menu so failures are visible.
			enum class Status : int {
				Idle = 0,
				NoNvidiaContainer = 1,
				AccessDenied = 2,
				Injected = 3,
				Failed = 4
			};

			bool EnsurePatched();
			void Tick(bool streamproofEnabled);
			Status GetStatus();
		}
	}
}
