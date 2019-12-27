#include "allegro5/allegro_audio.h"
#include "allegro5/allegro.h"
#include "Component.h"
#include <vector>
#include <algorithm>

namespace
{

}

struct Audio
{
	ALLEGRO_MIXER* mixer;
	ALLEGRO_VOICE* voice;
	std::vector<ALLEGRO_SAMPLE*> tracks;
	std::vector< ALLEGRO_SAMPLE_INSTANCE*> detachedInstances;

	component_list<ALLEGRO_SAMPLE_INSTANCE*> trackInstances;
};

namespace
{
	static ALLEGRO_SAMPLE_INSTANCE* CreateInst(Audio *a, uint trackId, bool loop, float volume, float pan)
	{
		ALLEGRO_SAMPLE* const track = a->tracks[trackId];
		ALLEGRO_SAMPLE_INSTANCE* const inst = al_create_sample_instance(track);

		al_set_sample_instance_gain(inst, volume);
		al_set_sample_instance_pan(inst, pan);
		al_set_sample_instance_playmode(inst, loop ? ALLEGRO_PLAYMODE_LOOP : ALLEGRO_PLAYMODE_ONCE);
		al_attach_sample_instance_to_mixer(inst, a->mixer);
		al_set_sample_instance_playing(inst, true);

		return inst;
	}
}

namespace aud
{
	Audio* Init()
	{
		ALLEGRO_VOICE* const voice = al_create_voice(44100, ALLEGRO_AUDIO_DEPTH_INT16, ALLEGRO_CHANNEL_CONF_2);

		if (voice)
		{
			ALLEGRO_MIXER* const mixer = al_create_mixer(44100, ALLEGRO_AUDIO_DEPTH_FLOAT32, ALLEGRO_CHANNEL_CONF_2);

			if (mixer)
			{
				if (al_attach_mixer_to_voice(mixer, voice))
				{
					Audio* const a = new Audio;

					a->mixer = mixer;
					a->voice = voice;

					return a;
				}
			}

			al_destroy_mixer(mixer);
		}

		al_destroy_voice(voice);

		return nullptr;
	}

	void Shutdown(Audio* a)
	{
		for (ALLEGRO_SAMPLE_INSTANCE* inst : a->detachedInstances)
			al_destroy_sample_instance(inst);

		for (ALLEGRO_SAMPLE_INSTANCE* inst : a->trackInstances.components)
			al_destroy_sample_instance(inst);

		for (ALLEGRO_SAMPLE* track : a->tracks)
			al_destroy_sample(track);

		al_destroy_mixer(a->mixer);
		al_destroy_voice(a->voice);

		delete a;
	}

	void Update(Audio* a)
	{
		a->detachedInstances.erase(std::remove_if(a->detachedInstances.begin(), a->detachedInstances.end(), [a](ALLEGRO_SAMPLE_INSTANCE* inst)
		{
			return !al_get_sample_instance_playing(inst);
		}), a->detachedInstances.end());
	}

	uint AddTrack(Audio* a, const char* file)
	{
		ALLEGRO_FILE* const f = al_fopen(file, "rb");
		uint trackId = ~0u;

		if (f)
		{
			ALLEGRO_SAMPLE* const track = al_load_sample_f(f, ".wav");

			if (track)
			{
				trackId = static_cast<uint>(a->tracks.size());
				a->tracks.emplace_back(track);
			}

			al_fclose(f);
		}

		return trackId;
	}

	void PlayTrackDetached(Audio* a, uint trackId, bool loop, float volume, float pan)
	{
		if (trackId < a->tracks.size())
		{
			a->detachedInstances.emplace_back(CreateInst(a, trackId, loop, volume, pan));
		}
	}

	void AttachTrack(Audio* a, uint objectId, uint track, float volume, bool loop)
	{
		if (track < a->tracks.size())
			a->trackInstances.add_to_object(objectId) = CreateInst(a, track, loop, volume, 0.0f);
	}

	void UpdatePan(Audio* a, uint objectId, float pan)
	{
		ALLEGRO_SAMPLE_INSTANCE* const* const instPtr = a->trackInstances.for_object(objectId);

		if (instPtr)
			al_set_sample_instance_pan(*instPtr, pan);
	}

	void UpdateVolume(Audio* a, uint objectId, float volume)
	{
		ALLEGRO_SAMPLE_INSTANCE* const* const instPtr = a->trackInstances.for_object(objectId);

		if (instPtr)
			al_set_sample_instance_gain(*instPtr, volume);
	}

	void RestartIfStopped(Audio* a, uint objectId)
	{	
		ALLEGRO_SAMPLE_INSTANCE* const* const instPtr = a->trackInstances.for_object(objectId);

		if (instPtr)
		{
			ALLEGRO_SAMPLE_INSTANCE* const inst = *instPtr;

			if (!al_get_sample_instance_playing(inst))
			{
				al_set_sample_instance_position(inst, 0);
				al_set_sample_instance_playing(inst, true);
			}
		}
	}

	void DestroyObjects(Audio* a, const std::vector<uint>& objectIds)
	{
		a->trackInstances.remove_objs(objectIds, [a](ALLEGRO_SAMPLE_INSTANCE* inst)
		{
			al_set_sample_instance_playing(inst, false);
			al_detach_sample_instance(inst);
			al_destroy_sample_instance(inst);
		});
	}
}
