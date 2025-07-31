# tnt
`tnt` is a simple console torrent client.

![demogif](https://github.com/alt-qi/tnt/blob/main/demo.gif)

### Building
```sh
mkdir build
cd build
cmake ..
make
```

### Usage
```sh
tnt [OPTION...] torrent_file

  -h, --help     Print usage
  -q, --quiet    Minimal output
  -o, --out arg  Output directory (working directory by default)
```

### Platform Support

#### Windows
On Windows Server 2003 and later, `tnt` automatically uses native Windows networking APIs for improved performance and compatibility:

- **HTTP Tracker Communication**: Uses WinHTTP instead of libcurl for tracker requests
- **TCP Connections**: Uses Winsock2 instead of Unix sockets for peer-to-peer communication

For Windows versions prior to Server 2003, the application falls back to the standard Unix/Linux networking implementation using libcurl and standard sockets.

#### Unix/Linux
Uses standard POSIX networking APIs:
- libcurl for HTTP tracker communication  
- Standard BSD sockets for TCP peer connections

### Features
- Self-written bencode decoding/encoding library and torrent file parser.
- Cross-platform networking with platform-specific optimizations:
  - WinHTTP on Windows Server 2003+
  - libcurl on Unix/Linux and older Windows versions
- Endgame mode is implemented.
- Multifile torrents are supported.

### Features to implement
- [x] Accept parameters from input args
- [x] Save downloaded pieces to disk
- [ ] Don't download already saved parts of file (load saved data)
- [x] Implement endgame mode
- [x] Reconnect to peers after spontaneous disconnect
- [x] Keep track of peers pieces availabilty 
- [ ] Add more command line options (like timeouts or announce override)
- [x] Rework piece downloading strategy
- [x] Add support for multifile torrents.
- [x] Move console graphics logic to separate classes
- [x] Move piece acquiring logic to separate class? 
- [ ] Add support for UDP trackers?
