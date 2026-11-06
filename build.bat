@echo off
setlocal
set ROOT=%~dp0
set OUT=%ROOT%build
set OBJ=%OUT%\obj
if not exist "%OUT%" mkdir "%OUT%"
if not exist "%OBJ%" mkdir "%OBJ%"

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (echo vswhere not found & exit /b 1)
set "VSPATH="
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSPATH=%%i"
if "%VSPATH%"=="" (echo MSVC with VC tools not found & exit /b 1)
call "%VSPATH%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1

set INC=/I"%ROOT%src" /I"%ROOT%vendor\imgui" /I"%ROOT%vendor\imgui\backends" /I"%ROOT%vendor\kiero" /I"%ROOT%vendor\minhook\include"
set DEFINES=/DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN /DWINVER=0x0A00 /D_WIN32_WINNT=0x0A00 /DKIERO_INCLUDE_D3D11=1 /DKIERO_INCLUDE_D3D12=1 /DKIERO_USE_MINHOOK=1
set CXXFLAGS=/nologo /c /std:c++20 /EHsc /O2 /W3 /utf-8 /DNDEBUG %DEFINES% %INC%

echo [1/4] project sources
cl %CXXFLAGS% /Fo"%OBJ%\\" ^
    "%ROOT%src\dllmain.cpp" "%ROOT%src\hooks.cpp" "%ROOT%src\overlay.cpp" ^
    "%ROOT%src\menu.cpp" "%ROOT%src\esp.cpp" "%ROOT%src\theme.cpp" ^
    "%ROOT%src\game\game.cpp" || exit /b 1

echo [2/4] vendor: imgui, kiero
cl %CXXFLAGS% /Fo"%OBJ%\\" ^
    "%ROOT%vendor\imgui\imgui.cpp" "%ROOT%vendor\imgui\imgui_draw.cpp" ^
    "%ROOT%vendor\imgui\imgui_tables.cpp" "%ROOT%vendor\imgui\imgui_widgets.cpp" ^
    "%ROOT%vendor\imgui\backends\imgui_impl_dx11.cpp" "%ROOT%vendor\imgui\backends\imgui_impl_dx12.cpp" ^
    "%ROOT%vendor\imgui\backends\imgui_impl_win32.cpp" ^
    "%ROOT%vendor\kiero\kiero.cpp" || exit /b 1

echo [3/4] vendor: minhook
cl /nologo /c /O2 /W3 /DWIN32_LEAN_AND_MEAN /I"%ROOT%vendor\minhook\include" /Fo"%OBJ%\\" ^
    "%ROOT%vendor\minhook\src\buffer.c" "%ROOT%vendor\minhook\src\hook.c" ^
    "%ROOT%vendor\minhook\src\trampoline.c" "%ROOT%vendor\minhook\src\hde\hde64.c" || exit /b 1

echo [4/4] link
link /nologo /DLL /OUT:"%OUT%\Kerix.dll" /PDB:"%OUT%\Kerix.pdb" "%OBJ%\*.obj" ^
    d3d12.lib dxgi.lib d3dcompiler.lib user32.lib || exit /b 1

echo done: %OUT%\Kerix.dll
endlocal
