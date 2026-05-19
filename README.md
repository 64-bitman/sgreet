# sgreet

A slightly nicer version of agreety for greetd

![screenshot](./screenshot.png)

## Build & Install

Build dependencies:
```
meson
json-c
ncurses
```

Install:
```
meson setup -C build
meson compile -C build
sudo meson install -C build
```

## Usage
```
Usage: sgreet [OPTIONS]

Options:
    -h,--help                   print this help message
    -v,--version                print version
    -s,--sessions {PATH}        session directory to search
    -p,--persist session,user   persist state
    -l,--logfile {PATH}         file to log messages to
    -a,--asterisks              show password in asterisks
```

Example command line
```
sgreet --sessions /usr/local/share/wayland-sessions --logfile /var/cache/sgreet/log.txt --persist user,session
```
Note that /var/cache/sgreet must already exist and be accessible by the greeter user for logging and persistence to work.

