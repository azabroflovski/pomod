# pomod-cli

A minimalistic, no-bullshit Pomodoro timer daemon.

## Motivation
F#ck GUI. We don't need your fancy buttons and modal windows.
So here's a tiny Pomodoro daemon that stays the hell out of your way.

## Installation
You know the drill

```sh
gcc -o daemon daemon.c
gcc -o pomod pomod.c
```

Or throw it all in a Makefile like a proper hacker. Up to you.

## Usage

Compile daemon.c and pomod.c, then do magic

```sh
./daemon
```

```
./pomod start
./pomod status # time left
./pomod stop
```

## Usage
Start the daemon

```sh
./daemon
```

CLI usage

```sh
./pomod start      # start a 25-minute pomodoro
./pomod status     # check how much time is left
./pomod stop       # stop the current session
```
