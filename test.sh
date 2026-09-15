#!/bin/sh
# runs a daemon in a temp dir and pokes it with the client
#   ./test.sh [path/to/pomod]
#   SLOW=1 ./test.sh     also waits out a real 1-minute pomodoro

pomod=${1:-./pomod}
dir=$(mktemp -d /tmp/pomod-test.XXXXXX) # short path, sun_path is ~104 bytes
sock=$dir/pomod.sock
export TMPDIR=$dir
unset XDG_RUNTIME_DIR
failed=0
daemon=

cleanup() {
    [ -n "$daemon" ] && kill "$daemon" 2>/dev/null
    rm -rf "$dir"
}
trap cleanup EXIT

pass() { printf 'ok    %s\n' "$1"; }
fail() { printf 'FAIL  %s\n' "$1"; failed=1; }

# check <exit code> <output pattern> <pomod args...>
check() {
    want_code=$1 want=$2
    shift 2
    got=$("$pomod" "$@" 2>&1)
    code=$?
    case $got in
        $want) [ "$code" = "$want_code" ] && pass "pomod $*" && return ;;
    esac
    fail "pomod $*"
    printf '      want: %s [%s]\n      got:  %s [%s]\n' "$want" "$want_code" "$got" "$code"
}

echo "# no daemon"
check 1 "pomod: can't reach $sock, is \`pomod daemon\` running?" status

"$pomod" daemon "echo done > $dir/hook" > "$dir/daemon.log" 2>&1 &
daemon=$!
i=0
while [ ! -S "$sock" ]; do
    i=$((i + 1))
    if [ $i -gt 50 ]; then
        echo "daemon didn't start:"
        cat "$dir/daemon.log"
        exit 1
    fi
    sleep 0.1
done

check 1 "pomod: daemon already running on $sock" daemon

echo "# idle"
check 0 "not running" status
check 0 "not running" stop
check 0 "not running" pause
check 0 "not running" resume
check 1 "pomod: unknown command 'foo'" foo

echo "# start"
check 1 "pomod: minutes should be 1..1440, got 'abc'" start abc
check 1 "pomod: minutes should be 1..1440, got '0'" start 0
check 1 "pomod: minutes should be 1..1440, got '1441'" start 1441
check 1 "pomod: minutes should be 1..1440, got '5x'" start 5x
check 0 "started, 50 min" start 50
check 0 "already running, ??:?? left" start
check 0 "??:?? left" status

echo "# pause"
check 0 "paused, ??:?? left" pause
left=$("$pomod" status)
sleep 1.5
check 0 "$left" status # the clock stands still
check 0 "already paused, ??:?? left" pause
check 0 "paused, ??:?? left" start
check 0 "resumed, ??:?? left" resume
check 0 "already running, ??:?? left" resume
check 0 "paused, ??:?? left" pause
check 0 "stopped" stop
check 0 "not running" status

if command -v nc > /dev/null; then
    echo "# raw socket"
    got=$(echo status | nc -U "$sock")
    [ "$got" = "not running" ] && pass "echo status | nc -U" || fail "echo status | nc -U, got '$got'"

    sleep 3 | nc -U "$sock" > /dev/null &
    sleep 0.2
    check 0 "not running" status # a silent client doesn't block others
fi

if [ -n "$SLOW" ]; then
    echo "# finish"
    check 0 "started, 1 min" start 1
    sleep 61.5
    check 0 "not running" status
    [ "$(cat "$dir/hook" 2>/dev/null)" = done ] && pass "hook ran" || fail "hook ran"
fi

echo "# shutdown"
kill "$daemon"
wait "$daemon"
code=$?
daemon=
[ "$code" = 0 ] && pass "daemon exits 0 on SIGTERM" || fail "daemon exits 0 on SIGTERM, got $code"
[ ! -e "$sock" ] && pass "socket removed" || fail "socket removed"

if [ "$failed" = 1 ]; then
    echo
    echo "daemon log:"
    cat "$dir/daemon.log"
fi
exit $failed
