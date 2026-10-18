#!/bin/sh
# Smoke test for the SoftGRE package inside an OpenWrt rootfs (musl, busybox, procd).
# Needs NET_ADMIN and NET_RAW. Run by docker/test.openwrt.sh.
set -eu
fail() { echo "openwrt_smoke: $*" >&2; exit 1; }

test -x /usr/sbin/softgred || fail "softgred not installed"
test -s /etc/softgred/softgred.conf || fail "default configuration missing"
head -n 1 /etc/init.d/softgred | grep -q '^#!/bin/sh /etc/rc.common' || fail "init script header"
grep -q 'USE_PROCD=1' /etc/init.d/softgred || fail "init script is not procd based"
test -x /etc/init.d/softgred || fail "init script not executable"
test ! -e /usr/share/softgred/webinterface || fail "dashboard must not be installed"
test ! -e /usr/share/softgred/dhcp-lease-hook.py || fail "DHCP hook must not be installed"
ls /lib/ld-musl-* >/dev/null 2>&1 || fail "not a musl system"
grep -q 'ld-musl' /usr/sbin/softgred || fail "binary is not linked against musl"

/usr/sbin/softgred --version | grep -q '^' || fail "--version failed"
if /usr/sbin/softgred --invalid-option 2>/dev/null; then fail "invalid option accepted"; fi

# Start the daemon in the foreground on loopback and query the status API.
conf=/tmp/softgred.conf
sed -e 's|^interface = .*|interface = lo|' -e 's|^bind-in = .*|bind-in = lo|' \
    -e 's|^log-file = .*|log-file = /tmp/softgred.log|' \
    -e 's|^pid-file = .*|pid-file = /tmp/softgred.pid|' /etc/softgred/softgred.conf >"$conf"
/usr/sbin/softgred -f -c "$conf" >/tmp/softgred.out 2>&1 &
pid=$!
trap 'kill "$pid" 2>/dev/null || true' EXIT
i=0
until printf "VERS\nQUIT\n" | nc 127.0.0.1 8888 2>/dev/null | grep -q 'RESULT: OK'; do
    i=$((i + 1))
    kill -0 "$pid" 2>/dev/null || { cat /tmp/softgred.out >&2; fail "daemon exited early"; }
    test "$i" -lt 20 || { cat /tmp/softgred.out >&2; fail "status API did not answer"; }
    sleep 1
done
kill -TERM "$pid"
wait "$pid" || true
trap - EXIT
echo 'openwrt_smoke: all checks passed'
