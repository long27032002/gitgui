# GitGui

GitGui is an open source Git desktop client built with C++20 and Qt 6. It is
designed for Linux desktop use and provides a graphical view of repository
changes, branches, history, and conflicts.

The interface takes inspiration from GitHub Desktop. GitGui is an independent
project and is not affiliated with, sponsored by, or endorsed by GitHub.

## Build

Requirements: CMake 3.21+, a C++20 compiler, Git, and Qt 6.4+ with Core, Gui,
Quick, Quick Controls 2, Concurrent, Test, and LinguistTools modules.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/src/app/gitgui
```

## Install from a release

Release assets include a Debian package, an Arch Linux package for x86_64, and
a portable Windows bundle. On EndeavourOS, install the Arch package with
`sudo pacman -U gitgui-0.1.0-1-x86_64.pkg.tar.zst` (replace the version with
the release you downloaded). Linux packages require Git and the Qt 6 runtime
modules listed by the package manager. On Windows, install Git for Windows and use the
`GitGui-windows-x64.zip` bundle; `gitgui.exe` by itself does not include Qt's
runtime libraries.

## License

GitGui is distributed under the MIT License. See [LICENSE](LICENSE).
