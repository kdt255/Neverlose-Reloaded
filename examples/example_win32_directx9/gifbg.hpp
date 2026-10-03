#pragma once

#include <string>

// An animated GIF behind the Spotify player.
//
// Spotify's own Canvas videos come from a private endpoint that would mean lifting the desktop
// client's access token, so this reads GIFs from disk instead: put files in `spotify_gif\` next to
// the executable and they are matched to whatever is playing, newest match first:
//
//     spotify_gif\<artist> - <title>.gif
//     spotify_gif\<title>.gif
//     spotify_gif\<artist>.gif
//     spotify_gif\default.gif
//
// Decoding, compositing and the frame timing all happen off the render thread; the UI thread only
// uploads finished frames and picks which one to draw.
namespace gifbg {

    void  set_enabled( bool on );
    bool  enabled( );

    // What is playing. Only the track key changes trigger a reload.
    void  set_track( const std::string& artist, const std::string& title );

    void  tick( float dt );  // once a frame on the UI thread: uploads new frames, advances time
    void* view( );           // the frame to draw now, or null when there is nothing to show
    bool  loaded( );         // a GIF is up and running (for the UI to say so)

    void  start( );
    void  stop( );
}
