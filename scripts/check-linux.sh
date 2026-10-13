#!/bin/sh
set -eu
src=$(CDPATH='' cd -- "$(dirname "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
cd "$src"
if test -f Makefile; then make distclean; fi
autoreconf --force --install
for mode in gcc clang sanitize minimal; do
    mkdir "$work/$mode"
    cd "$work/$mode"
    case "$mode" in
        gcc) "$src/configure" CC=gcc CFLAGS='-O2 -g -Werror' ;;
        clang) "$src/configure" CC=clang CFLAGS='-O2 -g -Werror' ;;
        sanitize) "$src/configure" CC=clang \
            CFLAGS='-O1 -g -Werror -fsanitize=address,undefined -fno-omit-frame-pointer' \
            LDFLAGS='-fsanitize=address,undefined' ;;
        minimal) "$src/configure" --disable-service CFLAGS='-O2 -Werror' ;;
    esac
    make -j2
    make check
    ./bin/softgred --help
    ./bin/softgred --version
    if ./bin/softgred --invalid-option; then exit 1; fi
    make install DESTDIR="$work/install-$mode"
    test -x "$work/install-$mode/usr/local/sbin/softgred"
    test -s "$work/install-$mode/usr/local/etc/softgred/softgred.conf"
    if test "$mode" = gcc; then
        "$src/configure" --enable-web CFLAGS='-O2 -Werror'
        make -j2
        make install DESTDIR="$work/install-web"
        test -s "$work/install-web/usr/local/share/softgred/webinterface/api.php"
        test -s "$work/install-web/usr/local/share/softgred/webinterface/server.py"
    fi
done
cd "$work/gcc"
make distcheck
