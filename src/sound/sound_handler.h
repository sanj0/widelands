/*
 * Copyright (C) 2005-2019 by the Widelands Development Team
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */

#ifndef WL_SOUND_SOUND_HANDLER_H
#define WL_SOUND_SOUND_HANDLER_H

#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>

#ifndef _MSC_VER
#include <unistd.h>
#endif

#include <SDL_mutex.h>

#include "random/random.h"
#include "sound/fxset.h"
#include "sound/constants.h"
#include "sound/songset.h"

extern class SoundHandler g_sound_handler;

/** The 'sound server' for Widelands.
 *
 * SoundHandler collects all functions for dealing with music and sound effects
 * in one class. It is similar in task - though not in scope - to well known
 * sound servers like gstreamer, EsounD or aRts. For the moment (and probably
 * forever), the only backend supported is SDL_mixer.
 *
 * \par Music
 *
 * Background music for different situations (e.g. 'Menu', 'Gameplay') is
 * collected in songsets. Each Songset contains references to one or more
 * songs in ogg format.
 *
 * Other classes can request to start or stop playing a certain songset,
 * changing the songset is provided as a convenience method. It is also
 * possible to switch to some other piece inside the same songset - but there
 * is \e no control over \e which song out of a songset gets played. The
 * selection is random.
 *
 * The files for the predefined system songsets
 * \li \c intro
 * \li \c menu
 * \li \c ingame
 *
 * must reside directly in the directory 'data/music' and must be named
 * SONGSET_XX.ogg where XX is a number from 00 to 99. All subdirectories of
 * 'data/music' will be considered to contain
 * ingame music. The music and sub-subdirectories found in them can be
 * arbitrarily named. This means: everything below data/music/ingame_01 can have
 * any name you want. All audio files below data/music/ingame_01 will be played as
 * ingame music.
 *
 * For more information about the naming scheme, see register_songs()
 *
 * You should be using the ogg format for music.
 *
 * \par Sound effects
 *
 *
 * Use register_fx() to record the file locations for each sound effect, to be loaded on demand.
 * Sound effects are kept in memory at all times once they have been loaded, to avoid delays from disk access.
 * The file naming scheme is the same as for the songs, and if there are multiple files for an effect, they are picked at random too.
 * Sound effects are categorized into multiple SoundType categories, so that the user can control which type of sounds to hear.
 *
 * For map objects, the abovementioned sound effects are synchronized with a work program or a
 * building/immovable/worker animation. For more information about this look at class
 * AnimationManager and see the online Lua reference for details.
 *
 *
 * \par Usage of callbacks
 *
 * SDL_mixer's way to notify the application of important sound events, e.g.
 * that a song is finished, are callbacks. While callbacks in and of themselves
 * are a fine thing, they can also be a pain in the body part with which we
 * usually touch our chairs.
 *
 * Problem 1:
 *
 * Callbacks must use global(or static) functions \e but \e not normal member
 * functions of a class. If you must know why: ask google. But how can a
 * static function share data with an instance of its own class? Usually not at
 * all.
 *
 * Fortunately, g_sound_handler already is a global variable,
 * and therefore accessible to global functions. So problem 1 disappears.
 *
 * Problem 2:
 *
 * Callbacks run in the caller's context. This means that when
 * music_finished_callback() is called, SDL_mixer and SDL_audio <b>will
 * be holding all of their locks!!</b> "So what?", you ask. The above means
 * that one \e must \e not call \b any SDL_mixer functions from inside the
 * callback, otherwise a deadlock is guaranteed. This indirectly does include
 * start_music(), stop_music() and of course change_music().
 * Unfortunately, that's just the functions we'd need to execute from the
 * callback. As if that was not enough, SDL_mixer internally uses
 * two separate threads, so you \e really don't want to play around with
 * locking.
 *
 * The only way around that resctriction is to send an SDL event(SDL_USEREVENT)
 * from the callback (non-sound SDL functions \e can be used). Then, later,
 * the main event loop will process this event \e but \e not in
 * SDL_mixer's context, so locking is no problem.
 *
 * Yes, that's just a tad ugly.
 *
 * No, there's no other way. At least none that I found.
 *
 * \par Stopping music without blocking
 *
 * When playing background music with SDL_mixer, we can fade the audio in/out.
 * Unfortunately, Mix_FadeOutMusic() will return immediately - but, as the music
 * is not yet stopped, starting a new piece of background music will block. So
 * the choice is to block (directly) after ordering to fade out or indirectly
 * when starting the next piece. Now imagine a fadeout-time of 30 seconds ...
 * and the user who is waiting for the next screen ......
 *
 * The solution is to work asynchronously, which is doable, as we already use a
 * callback to tell us when the audio is \e completely finished. So in
 * stop_music() (or change_music()) we just start the fadeout. The
 * callback then tells us when the audio has actually stopped and we can start
 * the next music. To differentiate between the two states we can just take a
 * peek with Mix_MusicPlaying() if there is music running. To make sure that
 * nothing bad happens, that check is not only required in change_music()
 * but also in start_music(), which causes the seemingly recursive call to
 * change_music() from inside start_music(). It really is not recursive, trust
 * me :-)
 */
// TODO(unknown): DOC: priorities
// TODO(unknown): DOC: play-or-not algorithm
// TODO(unknown): Environmental sound effects (e.g. wind)
// TODO(unknown): repair and reenable animation sound effects for 1-pic-animations

// This is used for SDL UserEvents to be handled in the main loop.
enum { CHANGE_MUSIC };

// Avoid clicks when starting/stopping music
constexpr int kMinimumMusicFade = 250;

class SoundHandler {
public:
	SoundHandler();
	~SoundHandler();

	void init();
	void shutdown();
	void save_config();
	void load_config();
	bool is_backend_disabled() const;
	void disable_backend();

	void register_fx(SoundType type, const std::string& dir,
	                       const std::string& basename,
	                       const std::string& fx_name);

	void play_fx(SoundType type, const std::string& fx_name,
	             uint8_t priority = kFxPriorityAlwaysPlay,
	             int32_t stereo_position = kStereoCenter, int distance = 0);

	void register_songs(const std::string& dir, const std::string& basename);
	void start_music(const std::string& songset_name, int fadein_ms = kMinimumMusicFade);
	void stop_music(int fadeout_ms = kMinimumMusicFade);
	void change_music(const std::string& songset_name = std::string(),
	                  int fadeout_ms = kMinimumMusicFade,
	                  int fadein_ms = kMinimumMusicFade);

	static void music_finished_callback();
	static void fx_finished_callback(int32_t channel);
	void handle_channel_finished(uint32_t channel);

	bool is_sound_enabled(SoundType type) const;
	void set_enable_sound(SoundType type, bool enable);
	int32_t get_volume(SoundType type) const;
	void set_volume(SoundType type, int32_t volume);

	/**
	 * Return the max value for volume settings. We use a function to hide
	 * SDL_mixer constants outside of sound_handler.
	 */
	int32_t get_max_volume() const {
		return MIX_MAX_VOLUME;
	}

private:
	void read_config();
	void register_music_and_system_sounds();

	// Prints an error and disables the sound system.
	void initialization_error(const char* const msg, bool quit_sdl);

	bool play_or_not(SoundType type, const std::string& fx_name, uint8_t priority);

	struct SoundOptions {
		explicit SoundOptions(int vol, const std::string& savename) : enabled(true), volume(vol), name(savename) {
			assert(!savename.empty());
			assert(vol >= 0);
			assert(vol <= MIX_MAX_VOLUME);
		}

		bool enabled;
		// Volume for sound effects or music (from 0 to get_max_volume())
		int volume;
		// Name for saving
		const std::string name;
	};

	/// Contains all options for sound types and music
	std::map<SoundType, SoundOptions> sound_options_;

	/// A collection of songsets
	using SongsetMap = std::map<std::string, std::unique_ptr<Songset>>;
	SongsetMap songs_;

	/// A collection of effect sets
	using FXsetMap = std::map<std::string, std::unique_ptr<FXset>>;
	std::map<SoundType, FXsetMap> fxs_;

	/// List of currently playing effects, and the channel each one is on
	/// Access to this variable is protected through fx_lock_ mutex.
	using ActivefxMap = std::map<uint32_t, std::string>;
	ActivefxMap active_fx_;

	/** Which songset we are currently selecting songs from - not regarding
	 * if there actually is a song playing \e right \e now.
	 */
	std::string current_songset_;

	/** The random number generator.
	 * \note The RNG here \e must \e not be the same as the one for the game
	 * logic!
	 */
	RNG rng_;

	/// Protects access to active_fx_ between callbacks and main code.
	SDL_mutex* fx_lock_;

	/** Can sounds be played?
	 * true = they mustn't be played (e.g. because hardware is missing) or the command line option --nosound was used.
	 * false = can be played
	 */
	bool backend_is_disabled_;
};

#endif  // end of include guard: WL_SOUND_SOUND_HANDLER_H
