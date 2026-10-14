#!/bin/sh
# Validate the deployed package, not the build directory.
set -eu
dpkg-query -W -f='${Status}\n' softgred | grep -qx 'install ok installed'
/usr/sbin/softgred --version
/usr/sbin/softgred --help >/dev/null
test -s /usr/share/softgred/webinterface/server.py
test -s /usr/share/softgred/run-services.py
test -x /usr/share/softgred/dhcp-lease-hook.py
test -s /usr/share/doc/softgred/LICENSE.md
test -s /usr/share/doc/softgred/docs/assets/softgre-logo.svg
test -s /usr/share/doc/softgred/docs/en_US/index.md
test -s /usr/share/doc/softgred/docs/en_US/dhcp.md
test -s /usr/share/doc/softgred/docs/assets/softgre-flow.svg
test -s /etc/logrotate.d/softgred
for unit in softgred softgred-web; do
    test -s "/lib/systemd/system/$unit.service" \
        || test -s "/usr/lib/systemd/system/$unit.service"
done
test -L /etc/systemd/system/multi-user.target.wants/softgred.service
test -L /etc/systemd/system/softgred.service.wants/softgred-web.service
python3 - <<'PY'
import ast
from pathlib import Path
for filename in ('/usr/share/softgred/run-services.py',
                 '/usr/share/softgred/webinterface/server.py',
                 '/usr/share/softgred/dhcp-lease-hook.py'):
    ast.parse(Path(filename).read_text())
PY
printf '%s\n' 'Installed daemon, dashboard, services, documentation and logrotate verified'
