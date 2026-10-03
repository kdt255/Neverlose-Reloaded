// C++/WinRT reaches for <experimental/coroutine> under C++17, and that header now hard-errors
// unless this is set. Nothing here uses co_await - only blocking .get() calls on the worker - so
// silencing it is exactly as safe as it looks, and it keeps the rest of the project on C++17.
#define _SILENCE_EXPERIMENTAL_COROUTINE_DEPRECATION_WARNINGS

#include "spotify.hpp"

#include <windows.h>
#include <shellapi.h>
#include <tlhelp32.h>
#include <d3d11.h>

#include <mmdeviceapi.h>
#include <audiopolicy.h>
#include <winhttp.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <thread>
#include <vector>

#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h> // range-for over the session IVectorView
#include <winrt/Windows.Media.Control.h>
#include <winrt/Windows.Storage.Streams.h>

#pragma comment( lib, "WindowsApp.lib" )
#pragma comment( lib, "Shell32.lib" )
#pragma comment( lib, "Ole32.lib" )
#pragma comment( lib, "Winhttp.lib" )

extern ID3D11Device* g_pd3dDevice;
// main.cpp: WIC decode -> immutable texture + SRV, with a full mip chain.
extern bool LoadTextureFromMemory( const void* data, size_t size, ID3D11ShaderResourceView** out );

namespace {

    using namespace winrt::Windows::Media::Control;
    using namespace winrt::Windows::Storage::Streams;
    using namespace winrt::Windows::Media;

    enum class command_id { toggle, next, previous, seek, volume, shuffle, repeat_cycle };

    struct command_t {
        command_id id;
        double     value = 0.0;
    };

    struct shared_t {
        std::mutex            lock;
        spotify::snapshot_t   snap;
        std::vector<uint8_t>  art;
        uint64_t              art_token = 0;

        std::mutex            cmd_lock;
        std::vector<command_t> commands;

        std::atomic<bool>     running{ false };
        std::thread           worker;
    } g;

    // ---- process ---------------------------------------------------------------------------

    DWORD spotify_pid( ) {
        HANDLE snap = ::CreateToolhelp32Snapshot( TH32CS_SNAPPROCESS, 0 );
        if ( snap == INVALID_HANDLE_VALUE )
            return 0;

        PROCESSENTRY32W entry{ };
        entry.dwSize = sizeof( entry );
        DWORD pid = 0;
        if ( ::Process32FirstW( snap, &entry ) ) {
            do {
                if ( ::_wcsicmp( entry.szExeFile, L"Spotify.exe" ) == 0 ) {
                    pid = entry.th32ProcessID;
                    break;
                }
            } while ( ::Process32NextW( snap, &entry ) );
        }
        ::CloseHandle( snap );
        return pid;
    }

    // ---- per-application volume (WASAPI) -----------------------------------------------------
    //
    // SMTC has no volume of its own; the slider drives Spotify's channel in the Windows mixer,
    // which is what a user means by "Spotify's volume" anyway. Spotify runs several processes, so
    // every session whose owner is a Spotify process is matched, not just the one with the PID the
    // process walk found first.

    bool is_spotify_process( DWORD pid ) {
        if ( pid == 0 )
            return false;
        HANDLE proc = ::OpenProcess( PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid );
        if ( !proc )
            return false;

        wchar_t path[ MAX_PATH ]{ };
        DWORD len = MAX_PATH;
        const bool ok = ::QueryFullProcessImageNameW( proc, 0, path, &len ) != FALSE;
        ::CloseHandle( proc );
        if ( !ok )
            return false;

        const wchar_t* leaf = ::wcsrchr( path, L'\\' );
        return ::_wcsicmp( leaf ? leaf + 1 : path, L"Spotify.exe" ) == 0;
    }

    template <typename Fn>
    bool for_each_spotify_volume( Fn&& fn ) {
        winrt::com_ptr<IMMDeviceEnumerator> enumerator;
        if ( FAILED( ::CoCreateInstance( __uuidof( MMDeviceEnumerator ), nullptr, CLSCTX_ALL,
                 __uuidof( IMMDeviceEnumerator ), enumerator.put_void( ) ) ) )
            return false;

        winrt::com_ptr<IMMDevice> device;
        if ( FAILED( enumerator->GetDefaultAudioEndpoint( eRender, eMultimedia, device.put( ) ) ) )
            return false;

        winrt::com_ptr<IAudioSessionManager2> manager;
        if ( FAILED( device->Activate( __uuidof( IAudioSessionManager2 ), CLSCTX_ALL, nullptr,
                 manager.put_void( ) ) ) )
            return false;

        winrt::com_ptr<IAudioSessionEnumerator> sessions;
        if ( FAILED( manager->GetSessionEnumerator( sessions.put( ) ) ) )
            return false;

        int count = 0;
        if ( FAILED( sessions->GetCount( &count ) ) )
            return false;

        bool any = false;
        for ( int i = 0; i < count; ++i ) {
            winrt::com_ptr<IAudioSessionControl> control;
            if ( FAILED( sessions->GetSession( i, control.put( ) ) ) )
                continue;

            auto control2 = control.try_as<IAudioSessionControl2>( );
            if ( !control2 )
                continue;

            DWORD pid = 0;
            if ( FAILED( control2->GetProcessId( &pid ) ) || !is_spotify_process( pid ) )
                continue;

            auto volume = control.try_as<ISimpleAudioVolume>( );
            if ( !volume )
                continue;

            fn( volume.get( ) );
            any = true;
        }
        return any;
    }

    bool read_volume( float& out ) {
        float found = -1.f;
        const bool any = for_each_spotify_volume( [ & ]( ISimpleAudioVolume* v ) {
            float level = 0.f;
            BOOL muted = FALSE;
            if ( SUCCEEDED( v->GetMasterVolume( &level ) ) && found < 0.f ) {
                v->GetMute( &muted );
                found = muted ? 0.f : level;
            }
        } );
        if ( any && found >= 0.f ) {
            out = found;
            return true;
        }
        return false;
    }

    void write_volume( float level ) {
        level = level < 0.f ? 0.f : ( level > 1.f ? 1.f : level );
        for_each_spotify_volume( [ & ]( ISimpleAudioVolume* v ) {
            v->SetMute( level <= 0.0005f ? TRUE : FALSE, nullptr );
            v->SetMasterVolume( level, nullptr );
        } );
    }

    // ---- media session -----------------------------------------------------------------------

    GlobalSystemMediaTransportControlsSession find_session(
        GlobalSystemMediaTransportControlsSessionManager const& manager ) {
        auto sessions = manager.GetSessions( );
        for ( auto const& session : sessions ) {
            std::wstring id{ session.SourceAppUserModelId( ) };
            for ( auto& c : id )
                c = ( wchar_t )::towlower( ( wint_t )c );
            if ( id.find( L"spotify" ) != std::wstring::npos )
                return session;
        }
        return nullptr;
    }

    std::vector<uint8_t> read_thumbnail(
        GlobalSystemMediaTransportControlsSessionMediaProperties const& props ) {
        std::vector<uint8_t> out;
        auto reference = props.Thumbnail( );
        if ( !reference )
            return out;

        auto stream = reference.OpenReadAsync( ).get( );
        const uint64_t size = stream.Size( );
        if ( size == 0 || size > 8ull * 1024ull * 1024ull )
            return out;

        DataReader reader( stream );
        reader.LoadAsync( ( uint32_t )size ).get( );
        out.resize( ( size_t )size );
        reader.ReadBytes( winrt::array_view<uint8_t>( out.data( ), out.data( ) + out.size( ) ) );
        return out;
    }

    void apply_commands( GlobalSystemMediaTransportControlsSession const& session ) {
        std::vector<command_t> pending;
        {
            std::lock_guard<std::mutex> l( g.cmd_lock );
            pending.swap( g.commands );
        }
        if ( pending.empty( ) )
            return;

        for ( const command_t& c : pending ) {
            try {
                switch ( c.id ) {
                case command_id::volume:
                    write_volume( ( float )c.value );
                    break;
                case command_id::toggle:
                    if ( session ) session.TryTogglePlayPauseAsync( ).get( );
                    break;
                case command_id::next:
                    if ( session ) session.TrySkipNextAsync( ).get( );
                    break;
                case command_id::previous:
                    if ( session ) session.TrySkipPreviousAsync( ).get( );
                    break;
                case command_id::seek:
                    // SMTC positions are in 100ns ticks.
                    if ( session ) session.TryChangePlaybackPositionAsync( ( int64_t )( c.value * 10'000'000.0 ) ).get( );
                    break;
                case command_id::shuffle:
                    if ( session ) session.TryChangeShuffleActiveAsync( c.value > 0.5 ).get( );
                    break;
                case command_id::repeat_cycle:
                    if ( session ) {
                        const auto mode = ( int )c.value == 1 ? MediaPlaybackAutoRepeatMode::List
                                        : ( int )c.value == 2 ? MediaPlaybackAutoRepeatMode::Track
                                                              : MediaPlaybackAutoRepeatMode::None;
                        session.TryChangeAutoRepeatModeAsync( mode ).get( );
                    }
                    break;
                }
            }
            catch ( ... ) {
                // A session can disappear between the poll and the command; the next poll re-syncs.
            }
        }
    }

    void worker_main( ) {
        winrt::init_apartment( winrt::apartment_type::multi_threaded );

        GlobalSystemMediaTransportControlsSessionManager manager{ nullptr };
        std::string art_key;   // title + album: the cover is only refetched when the track changes

        while ( g.running.load( std::memory_order_relaxed ) ) {
            spotify::snapshot_t snap;

            try {
                if ( !manager )
                    manager = GlobalSystemMediaTransportControlsSessionManager::RequestAsync( ).get( );

                const DWORD pid = spotify_pid( );
                auto session = manager ? find_session( manager ) : nullptr;

                if ( !manager )              snap.status = spotify::status_t::unsupported;
                else if ( !pid && !session ) snap.status = spotify::status_t::not_running;
                else if ( !session )         snap.status = spotify::status_t::idle;
                else                         snap.status = spotify::status_t::connected;

                if ( session ) {
                    auto info = session.GetPlaybackInfo( );
                    snap.playing = info.PlaybackStatus( ) ==
                        GlobalSystemMediaTransportControlsSessionPlaybackStatus::Playing;

                    auto controls = info.Controls( );
                    snap.can_play_pause = controls.IsPlayEnabled( ) || controls.IsPauseEnabled( ) ||
                                          controls.IsPlayPauseToggleEnabled( );
                    snap.can_next     = controls.IsNextEnabled( );
                    snap.can_previous = controls.IsPreviousEnabled( );
                    snap.can_seek     = controls.IsPlaybackPositionEnabled( );
                    snap.can_shuffle  = controls.IsShuffleEnabled( );
                    snap.can_repeat   = controls.IsRepeatEnabled( );

                    if ( auto shuffle = info.IsShuffleActive( ) )
                        snap.shuffle = shuffle.Value( );
                    if ( auto repeat = info.AutoRepeatMode( ) )
                        snap.repeat = repeat.Value( ) == MediaPlaybackAutoRepeatMode::Track ? spotify::repeat_t::one
                                    : repeat.Value( ) == MediaPlaybackAutoRepeatMode::List  ? spotify::repeat_t::all
                                                                                            : spotify::repeat_t::off;

                    auto timeline = session.GetTimelineProperties( );
                    snap.position = std::chrono::duration<double>( timeline.Position( ) ).count( );
                    snap.duration = std::chrono::duration<double>( timeline.EndTime( ) - timeline.StartTime( ) ).count( );

                    auto props = session.TryGetMediaPropertiesAsync( ).get( );
                    snap.title  = winrt::to_string( props.Title( ) );
                    snap.artist = winrt::to_string( props.Artist( ) );
                    snap.album  = winrt::to_string( props.AlbumTitle( ) );

                    const std::string key = snap.title + "\x1f" + snap.album + "\x1f" + snap.artist;
                    if ( key != art_key ) {
                        art_key = key;
                        std::vector<uint8_t> bytes = read_thumbnail( props );
                        std::lock_guard<std::mutex> l( g.lock );
                        g.art = std::move( bytes );
                        ++g.art_token;
                    }
                }
                else {
                    art_key.clear( );
                }

                snap.has_volume = pid != 0 && read_volume( snap.volume );

                apply_commands( session );
            }
            catch ( ... ) {
                snap = spotify::snapshot_t{ };
                snap.status = spotify::status_t::unsupported;
                manager = nullptr;
                art_key.clear( );
            }

            {
                std::lock_guard<std::mutex> l( g.lock );
                snap.art_token = g.art_token;
                g.snap = snap;
            }

            std::this_thread::sleep_for( std::chrono::milliseconds( 200 ) );
        }

        winrt::uninit_apartment( );
    }

    void push( command_id id, double value = 0.0 ) {
        std::lock_guard<std::mutex> l( g.cmd_lock );
        if ( g.commands.size( ) > 32 )
            g.commands.clear( ); // the worker is wedged; do not grow without bound
        g.commands.push_back( { id, value } );
    }

    // ---- UI-thread state ---------------------------------------------------------------------

    spotify::snapshot_t       s_published;
    ID3D11ShaderResourceView* s_art = nullptr;
    uint64_t                  s_art_uploaded = ( uint64_t )-1;
    double                    s_last_raw_position = -1.0;
    float                     s_seek_hold = 0.f;     // seconds left of a local position override
    double                    s_smooth_position = 0.0;
    std::string               s_last_title;
    float                     s_volume_hold = 0.f;   // seconds left of a local volume override
    float                     s_volume_local = 1.f;
}

// ---- lyrics ------------------------------------------------------------------------------
//
// lrclib.net: keyless, public, and built for third-party players. Its /api/get wants the exact
// artist / track / album / duration; when that misses we widen to a search. All of it runs on its
// own thread so a slow request never holds up the 200ms media poll, and nothing is requested at
// all until the user switches lyrics on.

namespace {

    struct lyrics_shared_t {
        std::mutex            lock;
        spotify::lyrics_t     data;
        uint64_t              version = 0;

        std::atomic<bool>     enabled{ false };
        std::atomic<bool>     running{ false };
        std::thread           worker;

        std::mutex            key_lock;
        std::string           wanted;      // the track the UI is asking about
    } gl;

    std::string url_encode( const std::string& in ) {
        static const char* hex = "0123456789ABCDEF";
        std::string out;
        out.reserve( in.size( ) * 3 );
        for ( unsigned char c : in ) {
            if ( ( c >= 'A' && c <= 'Z' ) || ( c >= 'a' && c <= 'z' ) || ( c >= '0' && c <= '9' ) ||
                 c == '-' || c == '_' || c == '.' || c == '~' ) {
                out.push_back( ( char )c );
            }
            else {
                out.push_back( '%' );
                out.push_back( hex[ c >> 4 ] );
                out.push_back( hex[ c & 0xf ] );
            }
        }
        return out;
    }

    bool http_get( const std::wstring& path, std::string& out ) {
        out.clear( );

        HINTERNET session = ::WinHttpOpen( L"kdt-menu/1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
            WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0 );
        if ( !session )
            return false;
        ::WinHttpSetTimeouts( session, 2500, 2500, 3000, 4000 ); // stop() joins this thread, so this
                                                                // is also the worst-case exit delay

        bool ok = false;
        if ( HINTERNET conn = ::WinHttpConnect( session, L"lrclib.net", INTERNET_DEFAULT_HTTPS_PORT, 0 ) ) {
            HINTERNET req = ::WinHttpOpenRequest( conn, L"GET", path.c_str( ), nullptr,
                WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE );
            if ( req ) {
                if ( ::WinHttpSendRequest( req, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                         WINHTTP_NO_REQUEST_DATA, 0, 0, 0 ) &&
                     ::WinHttpReceiveResponse( req, nullptr ) ) {

                    DWORD status = 0, size = sizeof( status );
                    ::WinHttpQueryHeaders( req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX );

                    if ( status == 200 ) {
                        char buffer[ 8192 ];
                        DWORD read = 0;
                        while ( ::WinHttpReadData( req, buffer, sizeof( buffer ), &read ) && read > 0 ) {
                            out.append( buffer, read );
                            if ( out.size( ) > 2u * 1024u * 1024u )
                                break;
                        }
                        ok = !out.empty( );
                    }
                }
                ::WinHttpCloseHandle( req );
            }
            ::WinHttpCloseHandle( conn );
        }
        ::WinHttpCloseHandle( session );
        return ok;
    }

    // Just enough JSON to pull one string field out - the response has a known, flat shape.
    bool json_string( const std::string& json, const char* field, std::string& out ) {
        out.clear( );
        const std::string key = std::string( "\"" ) + field + "\"";
        size_t at = json.find( key );
        if ( at == std::string::npos )
            return false;
        at = json.find( ':', at + key.size( ) );
        if ( at == std::string::npos )
            return false;
        ++at;
        while ( at < json.size( ) && ( json[ at ] == ' ' || json[ at ] == '\t' ) )
            ++at;
        if ( at >= json.size( ) || json[ at ] != '"' )
            return false; // null, or something that is not a string

        ++at;
        for ( ; at < json.size( ); ++at ) {
            const char c = json[ at ];
            if ( c == '"' )
                return true;
            if ( c != '\\' ) {
                out.push_back( c );
                continue;
            }
            if ( ++at >= json.size( ) )
                break;
            switch ( json[ at ] ) {
            case 'n': out.push_back( '\n' ); break;
            case 't': out.push_back( '\t' ); break;
            case 'r': break;
            case 'u': {
                if ( at + 4 >= json.size( ) )
                    return false;
                unsigned code = 0;
                for ( int i = 1; i <= 4; ++i ) {
                    const char h = json[ at + i ];
                    code = ( code << 4 ) | ( unsigned )( h <= '9' ? h - '0' : ( h | 32 ) - 'a' + 10 );
                }
                at += 4;
                // UTF-8, which is what ImGui wants; surrogate pairs are left as the replacement
                // character rather than half-encoded.
                if ( code < 0x80 ) {
                    out.push_back( ( char )code );
                }
                else if ( code < 0x800 ) {
                    out.push_back( ( char )( 0xC0 | ( code >> 6 ) ) );
                    out.push_back( ( char )( 0x80 | ( code & 0x3F ) ) );
                }
                else if ( code >= 0xD800 && code <= 0xDFFF ) {
                    out += "\xEF\xBF\xBD";
                }
                else {
                    out.push_back( ( char )( 0xE0 | ( code >> 12 ) ) );
                    out.push_back( ( char )( 0x80 | ( ( code >> 6 ) & 0x3F ) ) );
                    out.push_back( ( char )( 0x80 | ( code & 0x3F ) ) );
                }
                break;
            }
            default: out.push_back( json[ at ] ); break;
            }
        }
        return false;
    }

    void trim( std::string& v ) {
        size_t a = 0, b = v.size( );
        while ( a < b && ( unsigned char )v[ a ] <= ' ' ) ++a;
        while ( b > a && ( unsigned char )v[ b - 1 ] <= ' ' ) --b;
        v = v.substr( a, b - a );
    }

    // "[mm:ss.xx] text", with a line allowed to carry several stamps.
    void parse_lrc( const std::string& lrc, std::vector<spotify::lyric_line_t>& out ) {
        size_t at = 0;
        while ( at <= lrc.size( ) ) {
            size_t end = lrc.find( '\n', at );
            if ( end == std::string::npos )
                end = lrc.size( );
            std::string line = lrc.substr( at, end - at );
            at = end + 1;

            std::vector<double> stamps;
            size_t p = 0;
            while ( p + 1 < line.size( ) && line[ p ] == '[' ) {
                const size_t close = line.find( ']', p );
                if ( close == std::string::npos )
                    break;
                const std::string stamp = line.substr( p + 1, close - p - 1 );
                int mm = 0; double ss = 0.0;
                if ( ::sscanf_s( stamp.c_str( ), "%d:%lf", &mm, &ss ) == 2 )
                    stamps.push_back( mm * 60.0 + ss );
                p = close + 1;
            }
            if ( stamps.empty( ) )
                continue;

            std::string text = line.substr( p );
            trim( text );
            for ( double t : stamps )
                out.push_back( { t, text } );

            if ( end == lrc.size( ) )
                break;
        }

        std::sort( out.begin( ), out.end( ),
            []( const spotify::lyric_line_t& a, const spotify::lyric_line_t& b ) { return a.time < b.time; } );
    }

    void parse_plain( const std::string& text, std::vector<spotify::lyric_line_t>& out ) {
        size_t at = 0;
        while ( at <= text.size( ) ) {
            size_t end = text.find( '\n', at );
            if ( end == std::string::npos )
                end = text.size( );
            std::string line = text.substr( at, end - at );
            trim( line );
            out.push_back( { -1.0, line } );
            if ( end == text.size( ) )
                break;
            at = end + 1;
        }
    }

    void publish_lyrics( spotify::lyrics_state_t state, bool synced,
                         std::vector<spotify::lyric_line_t> lines ) {
        std::lock_guard<std::mutex> l( gl.lock );
        gl.data.state = state;
        gl.data.synced = synced;
        gl.data.lines = std::move( lines );
        ++gl.version;
    }

    bool fetch_for( const std::string& artist, const std::string& title, const std::string& album,
                    int duration ) {
        const std::string q = "artist_name=" + url_encode( artist ) + "&track_name=" + url_encode( title );
        std::string body;

        std::string path = "/api/get?" + q + "&album_name=" + url_encode( album ) +
                           "&duration=" + std::to_string( duration );
        std::wstring wide( path.begin( ), path.end( ) );
        if ( !http_get( wide, body ) ) {
            if ( !gl.running.load( std::memory_order_relaxed ) )
                return false;
            path = "/api/get?" + q;                       // album or duration was off by a second
            wide.assign( path.begin( ), path.end( ) );
            if ( !http_get( wide, body ) ) {
                if ( !gl.running.load( std::memory_order_relaxed ) )
                    return false;
                path = "/api/search?" + q;                // last resort: take the best match
                wide.assign( path.begin( ), path.end( ) );
                if ( !http_get( wide, body ) )
                    return false;
            }
        }

        std::string synced, plain;
        json_string( body, "syncedLyrics", synced );
        json_string( body, "plainLyrics", plain );

        std::vector<spotify::lyric_line_t> lines;
        if ( !synced.empty( ) ) {
            parse_lrc( synced, lines );
            if ( !lines.empty( ) ) {
                publish_lyrics( spotify::lyrics_state_t::ready, true, std::move( lines ) );
                return true;
            }
        }
        if ( !plain.empty( ) ) {
            parse_plain( plain, lines );
            if ( !lines.empty( ) ) {
                publish_lyrics( spotify::lyrics_state_t::ready, false, std::move( lines ) );
                return true;
            }
        }
        return false;
    }

    void lyrics_worker( ) {
        std::string served; // the track the published lyrics belong to

        while ( gl.running.load( std::memory_order_relaxed ) ) {
            std::string wanted;
            {
                std::lock_guard<std::mutex> l( gl.key_lock );
                wanted = gl.wanted;
            }

            if ( !gl.enabled.load( std::memory_order_relaxed ) ) {
                if ( !served.empty( ) ) {
                    served.clear( );
                    publish_lyrics( spotify::lyrics_state_t::off, false, { } );
                }
            }
            else if ( wanted != served ) {
                served = wanted;
                if ( wanted.empty( ) ) {
                    publish_lyrics( spotify::lyrics_state_t::none, false, { } );
                }
                else {
                    publish_lyrics( spotify::lyrics_state_t::loading, false, { } );

                    // key is artist \x1f title \x1f album \x1f duration
                    std::string parts[ 4 ];
                    size_t at = 0;
                    for ( int i = 0; i < 4; ++i ) {
                        const size_t sep = wanted.find( '\x1f', at );
                        parts[ i ] = wanted.substr( at, sep == std::string::npos ? sep : sep - at );
                        if ( sep == std::string::npos )
                            break;
                        at = sep + 1;
                    }

                    bool ok = false;
                    try {
                        ok = fetch_for( parts[ 0 ], parts[ 1 ], parts[ 2 ], ::atoi( parts[ 3 ].c_str( ) ) );
                    }
                    catch ( ... ) {
                        ok = false;
                    }
                    if ( !ok )
                        publish_lyrics( spotify::lyrics_state_t::none, false, { } );
                }
            }

            std::this_thread::sleep_for( std::chrono::milliseconds( 150 ) );
        }
    }

    // UI-thread copy, refreshed in tick() only when the worker bumps its version.
    spotify::lyrics_t s_lyrics;
    uint64_t          s_lyrics_version = ( uint64_t )-1;
}

namespace spotify {

    void set_lyrics_enabled( bool on ) {
        gl.enabled.store( on, std::memory_order_relaxed );
    }

    const lyrics_t& lyrics( ) { return s_lyrics; }

    int lyrics_index( double position ) {
        if ( !s_lyrics.synced || s_lyrics.lines.empty( ) )
            return -1;
        int found = -1;
        for ( size_t i = 0; i < s_lyrics.lines.size( ); ++i ) {
            if ( s_lyrics.lines[ i ].time <= position + 0.15 )
                found = ( int )i;
            else
                break;
        }
        return found;
    }
}

namespace spotify {

    void start( ) {
        if ( g.running.exchange( true ) )
            return;
        g.worker = std::thread( worker_main );
        gl.running.store( true );
        gl.worker = std::thread( lyrics_worker );
    }

    void stop( ) {
        if ( !g.running.exchange( false ) )
            return;
        if ( g.worker.joinable( ) )
            g.worker.join( );
        gl.running.store( false );
        if ( gl.worker.joinable( ) )
            gl.worker.join( );
        if ( s_art ) {
            s_art->Release( );
            s_art = nullptr;
        }
    }

    void tick( float dt ) {
        snapshot_t snap;
        std::vector<uint8_t> art;
        bool have_new_art = false;
        {
            std::lock_guard<std::mutex> l( g.lock );
            snap = g.snap;
            if ( g.art_token != s_art_uploaded ) {
                art = g.art;
                s_art_uploaded = g.art_token;
                have_new_art = true;
            }
        }

        if ( have_new_art ) {
            if ( s_art ) {
                s_art->Release( );
                s_art = nullptr;
            }
            if ( !art.empty( ) && g_pd3dDevice )
                LoadTextureFromMemory( art.data( ), art.size( ), &s_art );
        }

        // SMTC only republishes the position about once a second, so it is advanced locally in
        // between and re-anchored whenever a genuinely new value arrives. Without this the progress
        // bar ticks forward in visible one-second steps.
        const double raw = snap.position;
        if ( snap.title != s_last_title ) {
            s_last_title = snap.title;
            s_last_raw_position = -1.0;
            s_seek_hold = 0.f;
        }

        if ( s_seek_hold > 0.f ) {
            s_seek_hold -= dt;
            // Hold the spot the drag landed on until the app reports something near it. Snapping
            // back to the position it still had from before the seek is what read as lag.
            const double drift = raw > s_smooth_position ? raw - s_smooth_position : s_smooth_position - raw;
            if ( drift < 1.5 ) {
                s_seek_hold = 0.f;
                s_last_raw_position = raw;
                s_smooth_position = raw;
            }
            else if ( snap.playing ) {
                s_smooth_position += dt;
            }
        }
        else if ( raw != s_last_raw_position ) {
            s_last_raw_position = raw;
            s_smooth_position = raw;
        }
        else if ( snap.playing ) {
            s_smooth_position += dt;
        }
        if ( snap.duration > 0.0 && s_smooth_position > snap.duration )
            s_smooth_position = snap.duration;
        if ( s_smooth_position < 0.0 )
            s_smooth_position = 0.0;
        snap.position = s_smooth_position;

        // A dragged volume slider has to answer immediately; the mixer read that confirms it is a
        // poll behind, and letting it win would make the knob jump back under the cursor.
        if ( s_volume_hold > 0.f ) {
            s_volume_hold -= dt;
            snap.volume = s_volume_local;
            snap.has_volume = true;
        }

        // What the lyrics worker should be looking for. It only acts on this while the feature is
        // switched on, so an untouched toggle means no request ever leaves the machine.
        {
            std::string key;
            if ( snap.status == status_t::connected && !snap.title.empty( ) )
                key = snap.artist + "\x1f" + snap.title + "\x1f" + snap.album + "\x1f" +
                      std::to_string( ( int )( snap.duration + 0.5 ) );
            std::lock_guard<std::mutex> l( gl.key_lock );
            gl.wanted = key;
        }
        {
            std::lock_guard<std::mutex> l( gl.lock );
            if ( gl.version != s_lyrics_version ) {
                s_lyrics_version = gl.version;
                s_lyrics = gl.data;
            }
        }

        s_published = snap;
    }

    const snapshot_t& state( ) { return s_published; }

    void* art_view( ) { return ( void* )s_art; }

    void toggle_play( ) { push( command_id::toggle ); }
    void next( )        { push( command_id::next ); }
    void previous( )    { push( command_id::previous ); }
    void seek( double seconds ) {
        push( command_id::seek, seconds );
        // The bar has to land where it was dropped; SMTC will not report the new position for
        // another poll or two, and until then its old one would pull the fill back.
        s_smooth_position = seconds;
        s_seek_hold = 1.2f;
    }

    void set_volume( float v ) {
        s_volume_local = v < 0.f ? 0.f : ( v > 1.f ? 1.f : v );
        s_volume_hold = 0.6f;
        push( command_id::volume, s_volume_local );
    }

    void set_shuffle( bool on ) { push( command_id::shuffle, on ? 1.0 : 0.0 ); }

    void cycle_repeat( ) {
        const int next_mode = ( ( int )s_published.repeat + 1 ) % 3;
        push( command_id::repeat_cycle, ( double )next_mode );
    }

    void launch( ) {
        // Already up: bring its window forward instead of starting a second copy.
        if ( spotify_pid( ) != 0 ) {
            struct finder {
                static BOOL CALLBACK proc( HWND hwnd, LPARAM param ) {
                    DWORD pid = 0;
                    ::GetWindowThreadProcessId( hwnd, &pid );
                    if ( !is_spotify_process( pid ) || !::IsWindowVisible( hwnd ) )
                        return TRUE;
                    if ( ::GetWindow( hwnd, GW_OWNER ) != nullptr )
                        return TRUE;
                    *( HWND* )param = hwnd;
                    return FALSE;
                }
            };
            HWND window = nullptr;
            ::EnumWindows( finder::proc, ( LPARAM )&window );
            if ( window ) {
                if ( ::IsIconic( window ) )
                    ::ShowWindow( window, SW_RESTORE );
                ::SetForegroundWindow( window );
                return;
            }
        }

        // The spotify: protocol is registered by the desktop installer and by the Store build.
        if ( ( INT_PTR )::ShellExecuteW( nullptr, L"open", L"spotify:", nullptr, nullptr, SW_SHOWNORMAL ) > 32 )
            return;

        wchar_t appdata[ MAX_PATH ]{ };
        const DWORD n = ::GetEnvironmentVariableW( L"APPDATA", appdata, MAX_PATH );
        if ( n > 0 && n < MAX_PATH ) {
            const std::wstring exe = std::wstring( appdata ) + L"\\Spotify\\Spotify.exe";
            ::ShellExecuteW( nullptr, L"open", exe.c_str( ), nullptr, nullptr, SW_SHOWNORMAL );
        }
    }
}
