@REM Build for Visual Studio compiler. Run your copy of vcvars32.bat or vcvarsall.bat to setup command-line compiler.
@REM D3D11 only - everything it links ships with the Windows SDK, so the legacy DirectX SDK
@REM (DXSDK_DIR) is no longer on the include or library path.
@set OUT_DIR=Debug
@set OUT_EXE=example_win32_directx9
@set INCLUDES=/I..\.. /I..\..\backends
@set SOURCES=main.cpp gui.cpp blur.cpp shader_bg.cpp ..\..\backends\imgui_impl_dx11.cpp ..\..\backends\imgui_impl_win32.cpp ..\..\imgui*.cpp
@set LIBS=d3d11.lib dxgi.lib d3dcompiler.lib windowscodecs.lib
mkdir %OUT_DIR%
cl /nologo /Zi /MD /std:c++17 /utf-8 %INCLUDES% /D UNICODE /D _UNICODE %SOURCES% /Fe%OUT_DIR%/%OUT_EXE%.exe /Fo%OUT_DIR%/ /link %LIBS%
