# GitGui

[![Latest release](https://img.shields.io/github/v/release/long27032002/gitgui?display_name=tag)](https://github.com/long27032002/gitgui/releases/latest)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

**GitGui is a desktop client for working with Git repositories.** It combines a
change review and commit view with a multi-branch history graph, branch tools,
remote operations, and conflict resolution. The application is built with
C++20, Qt 6, and QML.

[Download the latest release](https://github.com/long27032002/gitgui/releases/latest) · [Tiếng Việt](README.vi.md)

GitGui is an independent project. Its interface takes inspiration from GitHub
Desktop; it is not affiliated with or endorsed by GitHub. GitGui uses the Git
command-line program for repository operations, so remotes and authentication
use your existing Git configuration, SSH agent, or credential helper.

## Features

- **Review and commit changes:** inspect file diffs, select files and individual
  changed lines, write commit messages, add co-authors, amend an eligible
  commit, and undo an eligible last commit.
- **Explore history:** view commits on a color-coded, multi-branch graph;
  inspect commit details and changed files; cherry-pick, revert, or squash a
  range of commits.
- **Manage branches:** create, switch, rename, and delete branches; merge with
  merge commit, squash, or fast-forward; rebase; and choose how to handle local
  changes when switching branches.
- **Work with remotes:** clone from a URL, fetch, pull, push, and force-push
  with a lease check.
- **Resolve conflicts:** review conflicted files, choose conflict sections in
  the app or open an external editor, then explicitly finish the merge.
- **Keep work recoverable:** discarded changes are backed up so they can be
  restored. Conflicted files are not staged automatically.
- **Use the interface your way:** switch between Vietnamese and English, and
  choose a dark or light theme.

## Download and install

Release binaries are currently provided for **x86_64**. Linux packages are
available for Arch-based distributions (including EndeavourOS) and Debian or
Ubuntu. A portable Windows bundle is also available.

### EndeavourOS and Arch Linux

Download the `gitgui-*-x86_64.pkg.tar.zst` asset from the
[latest release](https://github.com/long27032002/gitgui/releases/latest), then
install it from the directory where it was downloaded:

```sh
sudo pacman -U ./gitgui-*-x86_64.pkg.tar.zst
```

The package declares its Git and Qt runtime dependencies. You can also install
the package from the terminal by providing the exact filename shown in your
Downloads directory.

### Debian and Ubuntu

Download the `.deb` asset from the
[latest release](https://github.com/long27032002/gitgui/releases/latest), then
install it with:

```sh
sudo apt install ./gitgui-*-x86_64.deb
```

The package manager will install the declared runtime dependencies. If `apt`
reports a missing dependency, run `sudo apt --fix-broken install` and retry.

### Windows

Download and extract `GitGui-windows-x64.zip`, then run `gitgui.exe` from the
extracted folder. The ZIP includes Qt runtime libraries. Install
[Git for Windows](https://git-scm.com/download/win) first and make sure Git is
available on `PATH`; GitGui delegates repository operations to it. The
standalone `gitgui.exe` release asset does not include those Qt libraries, so
use the ZIP for normal use.

## Getting started

1. Install Git and, on Windows, configure your Git credential helper or SSH
   agent before connecting to a remote.
2. Start GitGui and open an existing repository, clone one from its URL, or
   create a new local repository.
3. In **Changes**, review the diff. Select the files and changed lines to
   include, enter a commit message, and commit.
4. In **History**, select a commit to inspect its details and use the available
   history actions. The graph shows local and remote branches together.
5. Use **View → Ngôn ngữ / Language** to change the interface language. Theme
   and language preferences are saved on the machine.

### Staging behavior

GitGui keeps its file and line selection in the app. At commit time it rebuilds
the Git index from that selection. If you staged content in another Git tool,
review the selection in GitGui before committing; the app may replace that
index state with the changes selected in its own view. New changes are selected
by default.

### Safety and limitations

- Force-push uses `--force-with-lease` and checks the upstream commit shown for
  confirmation.
- Amend and undo-last-commit actions are restricted to commits that have not
  been pushed.
- Discarding changes creates a local backup first; use the restore action to
  recover it.
- GitGui does not currently provide GitHub account sign-in, pull request
  management, or a list of repositories from your GitHub account. Clone a
  repository by entering its URL instead.
- Manual stash browsing and management is not available yet. GitGui can offer
  to stash local changes during a branch switch.

## Build from source

### Requirements

- CMake 3.21 or newer
- A C++20 compiler
- Git 2.28 or newer
- Qt 6.4 or newer with Core, Gui, Quick, Quick Controls 2, Concurrent, Test,
  and LinguistTools
- Ninja (recommended; another supported CMake generator can be used)

On EndeavourOS or Arch Linux, install the build dependencies with:

```sh
sudo pacman -S --needed base-devel cmake ninja git qt6-base qt6-declarative qt6-tools
```

On Debian or Ubuntu, install the build dependencies with:

```sh
sudo apt install build-essential cmake ninja-build git qt6-base-dev \
  qt6-declarative-dev qt6-tools-dev qt6-tools-dev-tools qt6-l10n-tools
```

### Compile and run

From the repository root:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/src/app/gitgui
```

To run the automated test suite after building:

```sh
ctest --test-dir build --output-on-failure
```

To build without test targets, configure with
`-DGITGUI_BUILD_TESTS=OFF`.

## License

GitGui is distributed under the [MIT License](LICENSE). Third-party software,
including Qt and Git, remains under its own license and terms.
