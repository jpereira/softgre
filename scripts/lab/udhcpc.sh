#!/bin/sh
# udhcpc hook for simulated AP clients: apply the lease, default route and record it.
case "$1" in
    deconfig)
        ip addr flush dev "$interface"
        ip link set "$interface" up
        ;;
    bound | renew)
        ip addr flush dev "$interface"
        ip addr add "$ip/$mask" dev "$interface"
        if test -n "$router"; then
            ip route replace default via "${router%% *}" dev "$interface"
        fi
        printf 'ip=%s/%s router=%s dns=%s\n' "$ip" "$mask" "$router" "$dns" >"$SOFTGRE_LEASE"
        ;;
esac
