# pomod

![tomatoes](assets/tomatoes.jpg)
<sub>Photo by snowday83 on [Pixabay](https://pixabay.com)</sub>

A minimalistic, no-bullshit Pomodoro timer daemon. In C.

## Motivation
I live in the terminal and don't want a GUI app just to count 25 minutes.

It's also a reason to write something real in C. No dependencies, one file,
libc and a unix socket.

## Build
You know the drill

```sh
make
```

No make? Fine

```sh
cc -o pomod pomod.c
```

## Usage
Start the daemon somewhere: spare terminal tab, tmux pane, `&`, whatever

```sh
./pomod daemon
```

Then

```sh
./pomod start      # 25-minute pomodoro
./pomod start 50   # or 50, if you're in the zone
./pomod status     # time left
./pomod pause      # someone's at the door
./pomod resume
./pomod stop       # give up
```

When time is up the daemon beeps. Want a real notification? Give it a command,
it goes to `sh` every time a pomodoro ends

```sh
# macOS
./pomod daemon 'osascript -e "display notification \"take a break\" with title \"pomod\""'

# linux
./pomod daemon 'notify-send pomod "take a break"'
```

## How it works
The daemon sits on a unix socket in `$XDG_RUNTIME_DIR` (or `$TMPDIR`, or `/tmp`).
One line of text in, one line out. So you don't even need the client

```sh
echo status | nc -U $TMPDIR/pomod.sock
```
