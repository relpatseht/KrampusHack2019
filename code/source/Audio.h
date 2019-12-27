#pragma once

#include <vector>

struct Audio;
typedef unsigned uint;

namespace aud
{
	Audio* Init();
	void Shutdown(Audio* a);
	void Update(Audio* a);

	uint AddTrack(Audio* a, const char* file);
	void PlayTrackDetached(Audio* a, uint track, bool loop = false, float volume = 1.0, float pan = 0.0);

	void AttachTrack(Audio* a, uint objectId, uint track, float volume = 1.0, bool loop = true);
	void UpdatePan(Audio* a, uint objectId, float pan);
	void UpdateVolume(Audio* a, uint objectId, float volume);
	void RestartIfStopped(Audio* a, uint objectId);

	void DestroyObjects(Audio* a, const std::vector<uint>& objectIds);
}