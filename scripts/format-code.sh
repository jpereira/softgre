#!/bin/sh
# The formatter image fixes tool versions through Debian bookworm and Prettier 3.6.2.
set -eu
root=$(CDPATH='' cd -- "$(dirname "$0")/.." && pwd)
cd "$root"
case ${1:-} in
    '')
        c_mode=-i
        py_mode=''
        js_mode=--write
        sh_mode=-w
        php_tool=phpcbf
        ;;
    --check)
        c_mode='--dry-run --Werror'
        py_mode=--check
        js_mode=--check
        sh_mode=-d
        php_tool=phpcs
        ;;
    *)
        echo 'Usage: scripts/format-code.sh [--check]' >&2
        exit 2
        ;;
esac
# The mode variables intentionally contain only fixed formatter options.
clang-format $c_mode src/*.c src/*.h tests/*.c
python3 scripts/format-includes.py $py_mode
black $py_mode --line-length 100 scripts/*.py scripts/lab/*.py tests/*.py share/webinterface/server.py
prettier "$js_mode" docs/assets/*.js docs/assets/*.css share/webinterface/*.js \
    share/webinterface/*.css share/webinterface/*.html .github/workflows/*.yml .prettierrc.json
shfmt "$sh_mode" -i 4 -ci -bn autogen.sh bump.sh scripts/*.sh scripts/lab/*.sh
# PHPCBF returns 1 when it successfully fixes formatting.
status=0
"$php_tool" -n --standard=PSR12 share/webinterface/api.php || status=$?
if test "$php_tool" = phpcbf && test "$status" = 1; then status=0; fi
exit "$status"
