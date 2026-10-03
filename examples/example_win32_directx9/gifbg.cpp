#include "gifbg.hpp"

#include <windows.h>
#include <wincodec.h>
#include <d3d11.h>
#include <wrl/client.h>

#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>
#include <vector>

#pragma comment( lib, "windowscodecs.lib" )

extern ID3D11Device*        g_pd3dDevice;
extern ID3D11DeviceContext* g_pd3dDeviceContext;

namespace {

    using Microsoft::WRL::ComPtr;

    // A decoded GIF frame: the whole canvas as it looks at that point in the animation, plus how
    // long it stays up. Frames are composited on the worker so the UI thread only ever uploads a
    // straight rectangle of pixels.
    struct frame_t {
        std::vector<uint8_t> bgra;
        float                delay = 0.1f;
    };

    struct clip_t {
        std::vector<frame_t> frames;
        UINT width = 0, height = 0;
    };

    struct shared_t {
        std::mutex        lock;
        clip_t            ready;        // handed to the UI thread
        uint64_t          version = 0;
        bool              have = false;

        std::mutex        key_lock;
        std::string       wanted;

        std::atomic<bool> enabled{ false };
        std::atomic<bool> running{ false };
        std::thread       worker;
    } g;

    // ---- decoding ------------------------------------------------------------------------------

    UINT metadata_uint( IWICMetadataQueryReader* reader, const wchar_t* name, UINT fallback ) {
        if ( !reader )
            return fallback;
        PROPVARIANT value;
        ::PropVariantInit( &value );
        UINT out = fallback;
        if ( SUCCEEDED( reader->GetMetadataByName( name, &value ) ) ) {
            if ( value.vt == VT_UI2 )      out = value.uiVal;
            else if ( value.vt == VT_UI1 ) out = value.bVal;
            else if ( value.vt == VT_UI4 ) out = value.ulVal;
        }
        ::PropVariantClear( &value );
        return out;
    }

    // Source over destination, both straight (non-premultiplied) BGRA.
    void blend_over( uint8_t* dst, const uint8_t* src ) {
        const unsigned sa = src[ 3 ];
        if ( sa == 0 )
            return;
        if ( sa == 255 ) {
            dst[ 0 ] = src[ 0 ]; dst[ 1 ] = src[ 1 ]; dst[ 2 ] = src[ 2 ]; dst[ 3 ] = 255;
            return;
        }
        const unsigned da = dst[ 3 ];
        const unsigned oa = sa + da * ( 255 - sa ) / 255;
        if ( oa == 0 ) {
            dst[ 0 ] = dst[ 1 ] = dst[ 2 ] = dst[ 3 ] = 0;
            return;
        }
        for ( int c = 0; c < 3; ++c )
            dst[ c ] = ( uint8_t )( ( src[ c ] * sa + dst[ c ] * da * ( 255 - sa ) / 255 ) / oa );
        dst[ 3 ] = ( uint8_t )oa;
    }

    bool decode( IWICImagingFactory* wic, const std::wstring& path, clip_t& out ) {
        ComPtr<IWICBitmapDecoder> decoder;
        if ( FAILED( wic->CreateDecoderFromFilename( path.c_str( ), nullptr, GENERIC_READ,
                 WICDecodeMetadataCacheOnLoad, decoder.GetAddressOf( ) ) ) )
            return false;

        UINT count = 0;
        if ( FAILED( decoder->GetFrameCount( &count ) ) || count == 0 )
            return false;
        count = count > 150 ? 150 : count; // a background loop, not a film

        ComPtr<IWICMetadataQueryReader> global;
        decoder->GetMetadataQueryReader( global.GetAddressOf( ) );
        UINT cw = metadata_uint( global.Get( ), L"/logscrdesc/Width", 0 );
        UINT ch = metadata_uint( global.Get( ), L"/logscrdesc/Height", 0 );

        if ( cw == 0 || ch == 0 ) {
            ComPtr<IWICBitmapFrameDecode> first;
            if ( FAILED( decoder->GetFrame( 0, first.GetAddressOf( ) ) ) ||
                 FAILED( first->GetSize( &cw, &ch ) ) )
                return false;
        }
        if ( cw == 0 || ch == 0 || cw > 2048 || ch > 2048 )
            return false;

        const size_t stride = ( size_t )cw * 4;
        const size_t canvas_bytes = stride * ch;
        if ( canvas_bytes * count > 96ull * 1024ull * 1024ull )
            count = ( UINT )( 96ull * 1024ull * 1024ull / canvas_bytes );
        if ( count == 0 )
            return false;

        std::vector<uint8_t> canvas( canvas_bytes, 0 );
        std::vector<uint8_t> saved;

        out.frames.clear( );
        out.frames.reserve( count );
        out.width = cw;
        out.height = ch;

        for ( UINT i = 0; i < count; ++i ) {
            if ( !g.running.load( std::memory_order_relaxed ) )
                return false;

            ComPtr<IWICBitmapFrameDecode> frame;
            if ( FAILED( decoder->GetFrame( i, frame.GetAddressOf( ) ) ) )
                break;

            ComPtr<IWICMetadataQueryReader> meta;
            frame->GetMetadataQueryReader( meta.GetAddressOf( ) );
            const UINT left = metadata_uint( meta.Get( ), L"/imgdesc/Left", 0 );
            const UINT top = metadata_uint( meta.Get( ), L"/imgdesc/Top", 0 );
            const UINT delay = metadata_uint( meta.Get( ), L"/grctlext/Delay", 10 );
            const UINT disposal = metadata_uint( meta.Get( ), L"/grctlext/Disposal", 0 );

            UINT fw = 0, fh = 0;
            if ( FAILED( frame->GetSize( &fw, &fh ) ) || fw == 0 || fh == 0 )
                break;

            ComPtr<IWICFormatConverter> conv;
            if ( FAILED( wic->CreateFormatConverter( conv.GetAddressOf( ) ) ) ||
                 FAILED( conv->Initialize( frame.Get( ), GUID_WICPixelFormat32bppBGRA,
                     WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom ) ) )
                break;

            std::vector<uint8_t> piece( ( size_t )fw * fh * 4 );
            if ( FAILED( conv->CopyPixels( nullptr, fw * 4, ( UINT )piece.size( ), piece.data( ) ) ) )
                break;

            if ( disposal == 3 )
                saved = canvas;

            for ( UINT y = 0; y < fh; ++y ) {
                const UINT cy = top + y;
                if ( cy >= ch )
                    break;
                for ( UINT x = 0; x < fw; ++x ) {
                    const UINT cx = left + x;
                    if ( cx >= cw )
                        break;
                    blend_over( &canvas[ cy * stride + cx * 4 ], &piece[ ( ( size_t )y * fw + x ) * 4 ] );
                }
            }

            frame_t emitted;
            emitted.bgra = canvas;
            emitted.delay = delay > 0 ? delay / 100.f : 0.1f;
            if ( emitted.delay < 0.02f )
                emitted.delay = 0.1f; // what browsers do with 0/1-hundredth frames
            out.frames.push_back( std::move( emitted ) );

            if ( disposal == 2 ) {
                for ( UINT y = 0; y < fh; ++y ) {
                    const UINT cy = top + y;
                    if ( cy >= ch )
                        break;
                    const UINT run = ( left + fw > cw ? cw - left : fw ) * 4;
                    ::memset( &canvas[ cy * stride + ( size_t )left * 4 ], 0, run );
                }
            }
            else if ( disposal == 3 && !saved.empty( ) ) {
                canvas = saved;
            }
        }

        return !out.frames.empty( );
    }

    std::wstring widen( const std::string& in ) {
        if ( in.empty( ) )
            return std::wstring( );
        const int n = ::MultiByteToWideChar( CP_UTF8, 0, in.c_str( ), ( int )in.size( ), nullptr, 0 );
        std::wstring out( ( size_t )n, L'\0' );
        ::MultiByteToWideChar( CP_UTF8, 0, in.c_str( ), ( int )in.size( ), out.data( ), n );
        return out;
    }

    // Windows will not take these in a file name, and a track title is full of them.
    std::wstring sanitise( std::wstring v ) {
        for ( wchar_t& c : v )
            if ( ::wcschr( L"\\/:*?\"<>|", c ) )
                c = L'_';
        return v;
    }

    void worker_main( ) {
        ::CoInitializeEx( nullptr, COINIT_MULTITHREADED );

        ComPtr<IWICImagingFactory> wic;
        ::CoCreateInstance( CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS( wic.GetAddressOf( ) ) );

        wchar_t dir[ MAX_PATH ]{ };
        ::GetCurrentDirectoryW( MAX_PATH, dir );
        const std::wstring folder = std::wstring( dir ) + L"\\spotify_gif\\";

        std::string served = "\x01"; // anything the first key cannot equal

        while ( g.running.load( std::memory_order_relaxed ) ) {
            std::string wanted;
            {
                std::lock_guard<std::mutex> l( g.key_lock );
                wanted = g.wanted;
            }
            const bool on = g.enabled.load( std::memory_order_relaxed );

            if ( !on ) {
                if ( served != "\x01" ) {
                    served = "\x01";
                    std::lock_guard<std::mutex> l( g.lock );
                    g.ready = clip_t{ };
                    g.have = false;
                    ++g.version;
                }
            }
            else if ( wanted != served && wic ) {
                served = wanted;

                std::wstring artist, title;
                const size_t sep = wanted.find( '\x1f' );
                if ( sep != std::string::npos ) {
                    artist = sanitise( widen( wanted.substr( 0, sep ) ) );
                    title = sanitise( widen( wanted.substr( sep + 1 ) ) );
                }

                const std::wstring candidates[] = {
                    ( !artist.empty( ) && !title.empty( ) ) ? folder + artist + L" - " + title + L".gif" : L"",
                    !title.empty( ) ? folder + title + L".gif" : L"",
                    !artist.empty( ) ? folder + artist + L".gif" : L"",
                    folder + L"default.gif",
                };

                clip_t clip;
                bool ok = false;
                for ( const std::wstring& candidate : candidates ) {
                    if ( candidate.empty( ) )
                        continue;
                    if ( ::GetFileAttributesW( candidate.c_str( ) ) == INVALID_FILE_ATTRIBUTES )
                        continue;
                    ok = decode( wic.Get( ), candidate, clip );
                    if ( ok )
                        break;
                }

                std::lock_guard<std::mutex> l( g.lock );
                g.ready = ok ? std::move( clip ) : clip_t{ };
                g.have = ok;
                ++g.version;
            }

            std::this_thread::sleep_for( std::chrono::milliseconds( 120 ) );
        }

        ::CoUninitialize( );
    }

    // ---- UI thread ------------------------------------------------------------------------------

    std::vector<ID3D11ShaderResourceView*> s_views;
    std::vector<float>                     s_delays;
    uint64_t                               s_version = ( uint64_t )-1;
    int                                    s_frame = 0;
    float                                  s_acc = 0.f;

    void release_views( ) {
        for ( ID3D11ShaderResourceView* v : s_views )
            if ( v )
                v->Release( );
        s_views.clear( );
        s_delays.clear( );
        s_frame = 0;
        s_acc = 0.f;
    }
}

namespace gifbg {

    void set_enabled( bool on ) { g.enabled.store( on, std::memory_order_relaxed ); }
    bool enabled( ) { return g.enabled.load( std::memory_order_relaxed ); }

    void set_track( const std::string& artist, const std::string& title ) {
        std::string key;
        if ( !title.empty( ) )
            key = artist + "\x1f" + title;
        std::lock_guard<std::mutex> l( g.key_lock );
        g.wanted = key;
    }

    void tick( float dt ) {
        clip_t clip;
        bool refreshed = false;
        {
            std::lock_guard<std::mutex> l( g.lock );
            if ( g.version != s_version ) {
                s_version = g.version;
                clip = std::move( g.ready );
                g.ready = clip_t{ };
                refreshed = true;
            }
        }

        if ( refreshed ) {
            release_views( );
            if ( g_pd3dDevice && !clip.frames.empty( ) ) {
                s_views.reserve( clip.frames.size( ) );
                s_delays.reserve( clip.frames.size( ) );

                D3D11_TEXTURE2D_DESC desc{ };
                desc.Width = clip.width;
                desc.Height = clip.height;
                desc.MipLevels = 1;
                desc.ArraySize = 1;
                desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
                desc.SampleDesc.Count = 1;
                desc.Usage = D3D11_USAGE_IMMUTABLE;
                desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

                for ( frame_t& f : clip.frames ) {
                    D3D11_SUBRESOURCE_DATA sub{ };
                    sub.pSysMem = f.bgra.data( );
                    sub.SysMemPitch = clip.width * 4;

                    ID3D11Texture2D* tex = nullptr;
                    if ( FAILED( g_pd3dDevice->CreateTexture2D( &desc, &sub, &tex ) ) || !tex )
                        continue;

                    ID3D11ShaderResourceView* srv = nullptr;
                    if ( SUCCEEDED( g_pd3dDevice->CreateShaderResourceView( tex, nullptr, &srv ) ) ) {
                        s_views.push_back( srv );
                        s_delays.push_back( f.delay );
                    }
                    tex->Release( );
                }
            }
        }

        if ( s_views.empty( ) )
            return;

        // Advance through however many frames this dt covers - a 2-hundredths frame should not eat
        // a whole render frame each.
        s_acc += dt < 0.f ? 0.f : ( dt > 0.25f ? 0.25f : dt );
        for ( int guard = 0; guard < 64; ++guard ) {
            const float d = s_delays[ ( size_t )s_frame ];
            if ( s_acc < d )
                break;
            s_acc -= d;
            s_frame = ( s_frame + 1 ) % ( int )s_views.size( );
        }
    }

    void* view( ) {
        if ( !g.enabled.load( std::memory_order_relaxed ) || s_views.empty( ) )
            return nullptr;
        return ( void* )s_views[ ( size_t )s_frame ];
    }

    bool loaded( ) { return !s_views.empty( ); }

    void start( ) {
        if ( g.running.exchange( true ) )
            return;
        g.worker = std::thread( worker_main );
    }

    void stop( ) {
        if ( !g.running.exchange( false ) )
            return;
        if ( g.worker.joinable( ) )
            g.worker.join( );
        release_views( );
    }
}
