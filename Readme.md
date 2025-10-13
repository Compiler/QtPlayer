
# MAC
- Use homebrew and install ffmpeg, pkg-config, opencv
- In environment: PATH=/opt/homebrew/bin:/opt/homebrew/sbin:/usr/bin:/bin:/usr/sbin:/sbin:/opt/local/bin:/opt/local/sbin

# Windows

## Windows Powershell
- git clone https://github.com/microsoft/vcpkg $env:USERPROFILE\vcpkg
- $env:VCPKG_ROOT="$env:USERPROFILE\vcpkg"
- & $env:VCPKG_ROOT\bootstrap-vcpkg.bat

## Windows set in initial config 
- CMAKE_RC_COMPILER="C:/Program Files (x86)/Windows Kits/10/bin/10.0.22621.0/x64/rc.exe"
- CMAKE_MT="C:/Program Files (x86)/Windows Kits/10/bin/10.0.22621.0/x64/mt.exe"
- CMAKE_TOOLCHAIN_FILE="C:/Users/<user>/<path_to_vcpkg>/vcpkg/scripts/buildsystems/vcpkg.cmake"

## If your runtimes are not found on Windows (Cannot find *.dll)
- Open Qt 6.10 MSVC Terminal
- run: "C:\Qt\6.10.0\msvc2022_64\bin\windeployqt.exe" --qmldir "<path to app root>" --compiler-runtime "<path_to_app_exe>"
- Move dlls from <path_to_build_folder>\build-QtPlayer-\vcpkg_installed\x64-windows\bin