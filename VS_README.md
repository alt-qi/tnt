# TNT Torrent Client - Visual Studio 2019 Project

This directory contains the Visual Studio 2019 project files for the TNT torrent client.

## Project Structure

The solution `tnt.sln` includes the following projects:

### Libraries (Static Libraries)
- **hash_library** - SHA1 implementation
- **bencode** - Bencode encoding/decoding library
- **torrent_file** - Torrent file parsing
- **tcp_connection** - TCP networking layer
- **peer_connection** - Peer protocol implementation
- **piece_storage** - Piece storage and management
- **peer_downloader** - Download logic
- **conductor** - Download coordination
- **visuals** - Console UI components
- **torrent_tracker** - Tracker communication

### Main Executable
- **tnt** - Main console application

## Supported Configurations

- **Debug/Release**: Standard debug and optimized release builds
- **x86/x64**: Both 32-bit and 64-bit architectures

## Dependencies

The project uses the following external dependencies managed via vcpkg:
- **cpr** - C++ HTTP requests library
- **cxxopts** - Command line option parsing

## Setup Instructions

### Prerequisites
1. Visual Studio 2019 (v142 toolset)
2. Windows SDK 10.0
3. vcpkg package manager

### vcpkg Setup
1. Install vcpkg if not already installed:
   ```bash
   git clone https://github.com/Microsoft/vcpkg.git
   cd vcpkg
   .\bootstrap-vcpkg.bat
   ```

2. Integrate vcpkg with Visual Studio:
   ```bash
   .\vcpkg integrate install
   ```

3. Install dependencies for both architectures:
   ```bash
   .\vcpkg install cpr:x86-windows cpr:x64-windows
   .\vcpkg install cxxopts:x86-windows cxxopts:x64-windows
   ```

### Build Instructions
1. Open `tnt.sln` in Visual Studio 2019
2. Select your desired configuration (Debug/Release) and platform (x86/x64)
3. Build the solution (Ctrl+Shift+B)

The built executable will be placed in:
- `bin\Win32\Debug\` or `bin\Win32\Release\` for x86 builds
- `bin\x64\Debug\` or `bin\x64\Release\` for x64 builds

## Platform-Specific Features

The Windows version includes optimizations:
- **WinHTTP** for HTTP tracker communication (Server 2003+)
- **Winsock2** for TCP peer connections
- Automatic fallback to cross-platform implementations when needed

## Project Dependencies

The projects have the following dependency chain:
```
tnt (executable)
├── hash_library
├── bencode
├── torrent_file
├── tcp_connection
├── peer_connection (depends on tcp_connection)
├── piece_storage (depends on hash_library)
├── peer_downloader (depends on peer_connection, piece_storage)
├── conductor (depends on peer_downloader)
├── visuals
└── torrent_tracker (depends on bencode)
```

The main executable project references all library projects and will build them in the correct order.

## Notes

- All projects use C++20 standard
- Output directories are configured to use a common `bin` and `obj` folder structure
- Project GUIDs are fixed to ensure consistent references
- Windows-specific libraries (winhttp.lib, ws2_32.lib) are automatically linked