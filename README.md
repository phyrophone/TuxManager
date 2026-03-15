# Tux Manager

A Linux Task Manager alternative built with Qt6, inspired by the Windows Task Manager but designed to go further - providing deep visibility into system processes, performance metrics, users, and services.

## Memory view
![Screenshot](screenshots/readme.png)
## CPU view
![Screenshot](screenshots/cpu.png)
## GPU view
![Screenshot](screenshots/gpu.png)

## Building

### qmake

```bash
mkdir build && cd build
qmake ..
make -j$(nproc)
./tux-manager
```

## License

GPL-3.0-or-later
