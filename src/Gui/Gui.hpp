#pragma once
#include <Includes/Includes.hpp>

using namespace std;

namespace Gui {

	struct capture_exclusion_status_t {
		bool attempted = false;
		bool active = false;
		bool layered = false;   // layered windows ignore the capture exclusion
		DWORD affinity = 0;
		DWORD last_error = 0;
	};

	capture_exclusion_status_t GetCaptureExclusionStatus();

	void Rendering( );
	void ShutdownStreamProofForUnload();
	inline DWORD ImGuiWindowFlags = ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

}
