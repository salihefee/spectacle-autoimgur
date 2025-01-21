# spectacle-autoimgur
[![CMake on a single platform](https://github.com/salihefee/spectacle-autoimgur/actions/workflows/cmake-single-platform.yml/badge.svg)](https://github.com/salihefee/spectacle-autoimgur/actions/workflows/cmake-single-platform.yml)

a simple and probably horrible program i wrote for myself to automatically upload screenshots taken by spectacle to imgur and copy the link to my clipboard. I use a systemd service to launch this at boot.


only works with wayland.
### usage:
`spectacle_autoimgur <directory_to_watch> <client_id>`

### how it works:
- uses inotify to watch for filesystem changes
- uses libcurl to upload the screenshot to imgur
- uses wl-clipboard to copy the imgur link to your clipboard

if you also decide to use this for some reason, dont forget to set `Environment=XDG_RUNTIME_DIR=/run/user/1000` in the systemd service.