#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Real Spotify, driven through Windows' own media session API (SMTC - the one behind the volume
// overlay), plus WASAPI for the per-application volume.
//
// Deliberately NOT the Spotify Web API: its playback endpoints (/me/player/play, /pause, /next)
// are Premium-only and need an OAuth round trip, a registered client id and a network connection.
// Talking to the desktop app through the OS needs none of that and behaves identically on Spotify
// Free and on Premium.
//
// Everything here is polled on a worker thread and published as an immutable snapshot, so the
// render thread never blocks on WinRT or COM.
namespace spotify {

    enum class status_t {
        unsupported,  // no session manager - too old a Windows, or the API refused us
        not_running,  // Spotify is not open
        idle,         // Spotify is open but has not registered a media session yet
        connected
    };

    enum class repeat_t { off, all, one };

    struct snapshot_t {
        status_t    status = status_t::unsupported;
        bool        playing = false;
        bool        shuffle = false;
        repeat_t    repeat = repeat_t::off;

        bool        can_play_pause = false;
        bool        can_next = false;
        bool        can_previous = false;
        bool        can_seek = false;
        bool        can_shuffle = false;
        bool        can_repeat = false;

        std::string title, artist, album;
        double      position = 0.0;   // seconds, smoothed on the UI thread between SMTC updates
        double      duration = 0.0;   // seconds
        float       volume = 1.0f;    // Spotify's own mixer level, -1 when it could not be read
        bool        has_volume = false;
        uint64_t    art_token = 0;    // bumps whenever the cover changes
    };

    // ---- lyrics ------------------------------------------------------------------------------
    //
    // Spotify's media session carries no lyrics, and its own lyrics endpoint is a private API that
    // would mean lifting the desktop client's access token. These come from lrclib.net instead - a
    // public, keyless service built for exactly this - and only while the feature is switched on,
    // so nothing about what is playing leaves the machine until the user asks for it.
    struct lyric_line_t {
        double      time;   // seconds, < 0 for an unsynced line
        std::string text;
    };

    enum class lyrics_state_t { off, loading, ready, none };

    struct lyrics_t {
        lyrics_state_t            state = lyrics_state_t::off;
        bool                      synced = false;
        std::vector<lyric_line_t> lines;
    };

    void            set_lyrics_enabled( bool on );
    const lyrics_t& lyrics( );
    int             lyrics_index( double position ); // active line, -1 before the first one

    void start( );
    void stop( );

    // Once a frame on the UI thread: republishes the worker's snapshot, advances the playback
    // position between the (roughly per-second) SMTC updates, and uploads new cover art.
    void tick( float dt );

    const snapshot_t& state( );
    void*             art_view( );  // ImTextureID for the current cover, or null

    void toggle_play( );
    void next( );
    void previous( );
    void seek( double seconds );
    void set_volume( float v );
    void set_shuffle( bool on );
    void cycle_repeat( );
    void launch( );      // start Spotify, or bring it to the front if it is already up
}
