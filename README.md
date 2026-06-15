# Match Game

A game built on C++ and SFML about matching gems.

## How to Build & Launch on Windows 11 using Ubuntu WSL

### Prerequisites

- Windows 11 with [Windows Subsystem for Linux (WSL)](https://learn.microsoft.com/en-us/windows/wsl/install)
- Ubuntu distribution installed via WSL
- [Git](https://git-scm.com/)
- [CMake](https://cmake.org/)
- [g++ (g++-11 or newer recommended)](https://packages.ubuntu.com/search?keywords=g++)
- [SFML 2.5 or newer](https://www.sfml-dev.org/download.php)
- (Optional) [Visual Studio Code](https://code.visualstudio.com/) with the "Remote - WSL" extension for code editing

### Steps

#### 1. Open WSL Terminal

- Open either your Start Menu or Windows Terminal, and select Ubuntu.

#### 2. Install Build Tools and Dependencies

```bash
sudo apt update
sudo apt install build-essential cmake git libsfml-dev
```

This installs the C++ compiler, CMake, Git, and SFML libraries.

#### 3. Clone the Repository

```bash
git clone https://github.com/via-mss/Match.git
cd Match
```

#### 4. Build the Project with CMake

```bash
mkdir build
cd build
cmake ..
make
```

- If the build completes successfully, the game executable should be in the `build` directory.

#### 5. Run the Game

```bash
./3InARow
```
- Or use the actual executable name if different.

---

### Notes

- If you get errors related to missing SFML libraries, make sure `libsfml-dev` is installed and visible to both compile-time and run-time environments.
- If you cloned submodules, make sure to initialize them:

```bash
git submodule update --init --recursive
```

- The above steps assume the project uses standard CMake and the executable is called `Match`. If your actual output binary has a different name, run that instead.

### Troubleshooting

- If you receive errors about missing display, make sure you have an X11 server running on Windows such as [VcXsrv](https://sourceforge.net/projects/vcxsrv/) or [X410](https://x410.dev/).
- Launch your X server in Windows before running the game in WSL, and ensure the environment variable is set:

```bash
export DISPLAY=$(cat /etc/resolv.conf | grep nameserver | awk '{print $2}'):0
```

- (Optionally, consider [WSLg](https://devblogs.microsoft.com/commandline/announcing-windows-subsystem-for-linux-gui/) on Windows 11, which allows running Linux GUI applications directly in WSL without extra setup.)

---

## License

See [LICENSE](LICENSE) for details.
