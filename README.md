# spectacle-autoimgur
a simple and probably horrible program i wrote for myself to automatically upload screenshots taken by spectacle to imgur and copy the link to my clipboard. I use a systemd service to launch this at boot.

only works with wayland.

### usage:
`spectacle_autoimgur <directory_to_watch> <client_id>`

if you also decide to use this for some reason, dont forget to set `Environment=XDG_RUNTIME_DIR=/run/user/1000` in the systemd service.