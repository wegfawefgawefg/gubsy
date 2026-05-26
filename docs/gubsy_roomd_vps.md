# gubsy-roomd VPS Setup

`gubsy-roomd` is the small HTTP directory service used by the Gubsy server
browser. It lists live rooms and returns session metadata. It does not relay
gameplay traffic.

## Build

From the Gubsy repo:

```sh
./scripts/build.sh
```

The server binary is:

```sh
build/gubsy-roomd
```

## Install On Debian

Copy or clone the repo onto the VPS, build it, then run:

```sh
./scripts/install_gubsy_roomd_debian.sh
sudo systemctl restart gubsy-roomd
```

The service reads:

```sh
/etc/gubsy-roomd.env
```

Default config:

```sh
GUBSY_ROOMD_HOST=0.0.0.0
GUBSY_ROOMD_PORT=8788
```

## Check

```sh
curl http://127.0.0.1:8788/health
curl http://127.0.0.1:8788/rooms
```

Expected health response:

```json
{"ok":true}
```

## Firewall

Open the configured TCP port if the VPS firewall is enabled:

```sh
sudo ufw allow 8788/tcp
```

## HTTPS

The first service speaks plain HTTP. For a public server, put nginx or caddy in
front of it and expose HTTPS to clients. Keep `gubsy-roomd` bound to localhost
behind the reverse proxy if you do that.

## Logs

`gubsy-roomd` writes structured JSON lines to stdout. With systemd:

```sh
journalctl -u gubsy-roomd -f
```

