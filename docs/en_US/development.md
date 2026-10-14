# Development and testing

All Dockerfiles are in `docker/`. Run build commands from the project root and keep `.`
as the build context; the root `.dockerignore` excludes Git metadata and generated
artifacts. CI runs every check on this page for each push and pull request.

## Unit and compiler checks

`make check` exercises the bounded parser, every truncation of valid packet fixtures, IPv4
options, outer VLAN tags, invalid GRE formats, seeded malformed inputs, typed
configuration, bridge mappings, hash indexes, client capacity, rollback, PID locks, API
commands, TCP fragmentation and peer disconnects. Tests use mock kernel operations for
state-machine faults and real loopback sockets for the API; `test-netlink` checks encoded
attributes, nesting and message bounds.

```sh
docker build -f docker/Dockerfile.test -t softgre-test .
docker run --rm softgre-test scripts/check-linux.sh
docker run --rm softgre-test python3 tests/test_web.py
docker run --rm softgre-test scripts/check-installed-package.sh
```

`check-linux.sh` builds out of tree with GCC and Clang and `-Werror`, runs ASan and
UBSan, checks `--disable-service --disable-web`, CLI exit codes and staged installation,
then runs `make distcheck`. `test_web.py` tests real HTTP connections, backend framing,
errors, limits and the overview; PHP compatibility tests run when PHP is installed.

## Kernel integration tests

The integration suites need a disposable network namespace and root networking
privileges, which the test container provides:

```sh
docker run --rm --cap-add NET_ADMIN --cap-add NET_RAW \
  -e SOFTGRE_TEST_NAMESPACE=isolated softgre-test \
  python3 tests/integration.py /usr/sbin/softgred
docker run --rm --privileged -e SOFTGRE_TEST_NAMESPACE=isolated softgre-test \
  python3 tests/dhcp_integration.py /usr/sbin/softgred
```

`integration.py` verifies real gretap and VLAN interfaces, bridge attachment, BPF
capture, 10000 repeated packets without duplicate rules, outer VLANs, API framing, signal
cleanup, unrelated firewall preservation, PID locking and background startup failures.
It prints the packet injection duration, a repeatable smoke measurement rather than a
throughput benchmark. `dhcp_integration.py` builds real APs and clients in namespaces
and is described in [DHCP and subscriber services](dhcp.md#end-to-end-dhcp-test).

## Lab and gateway checks

```sh
docker build -f docker/Dockerfile.lab -t softgre-lab:ubuntu24.04 .
scripts/check-lab.sh
docker build -f docker/Dockerfile.lab --target gateway -t softgre-gateway:ubuntu24.04 .
scripts/check-gateway.sh
```

`check-lab.sh` starts a temporary single-container lab and checks systemd services,
provisioning, the dashboard, restarts and a clean shutdown. `check-gateway.sh` starts the
gateway with two AP containers on a temporary Docker network and checks leases, gateway
pings, the lease hook with option 82 and the dashboard. Both remove their containers.

## Source formatting

Use four spaces for C, Python and shell, and two spaces for frontend assets and YAML.
Code wraps at 100 columns where possible; keep long URLs, commands, output examples and
user-facing strings intact. The formatter image provides clang-format 14, Black, shfmt,
PHP_CodeSniffer and Prettier 3.6.2:

```sh
docker build -f docker/Dockerfile.format -t softgre-format .
docker run --rm softgre-format scripts/format-code.sh --check
docker run --rm -v "$PWD:/src" softgre-format scripts/format-code.sh
```

The last command rewrites the mounted workspace on a local Docker engine. With a remote
Docker context, run the formatter in its copied tree and copy the results back.

## Commit hooks

The repository ships a `commit-msg` hook in `.githooks/` that strips any Claude or
Anthropic co-authorship or "Generated with" trailer, keeping that attribution out of the
history. Enable it once per clone:

```sh
git config core.hooksPath .githooks
```

## Releases

Version 2.0.1 is defined in `VERSION`. Autoconf reads that file; `bump.sh` also updates
the Debian changelog and version references in the README and these guides.

```sh
./bump.sh --check
./bump.sh patch
```

Use `major`, `minor`, `patch` (default) or an explicit `MAJOR.MINOR.PATCH`. A bump must
increase the version; it does not commit, tag or push. Commit the bump, then push a
matching tag:

```sh
release_version=$(cat VERSION)
git tag "v$release_version"
git push origin master "v$release_version"
```

The release workflow validates the tagged source with the full Linux CI, builds and tests
each distribution's package inside a matching image with `scripts/build-deb.sh`, and
uploads the packages and `SHA256SUMS` to the GitHub release only after every job
succeeds. The same tag triggers the container workflow, which validates the source and
publishes the version tag and `latest` to GitHub Packages, with the source, commit,
version and license in OCI labels. Run either workflow
manually with an existing `vX.Y.Z` tag to republish it.

## Documentation site

This site is built from the Markdown pages in `docs/en_US/` with Python's standard
library: no Node, third-party assets, tracking, remote fonts or external JavaScript. The
page order is defined in `scripts/build-docs.py`. The builder supports headings,
paragraphs, ordered and unordered lists, tables, fenced code, inline code, bold text,
links between pages (`page.md#section`) and images from `docs/assets/`; it escapes raw
HTML. The light theme is the default and the theme button stores the reader's choice.

```sh
python3 scripts/build-docs.py
python3 tests/test_docs.py
python3 -m http.server 8000 --directory _site
```

The Pages workflow publishes `_site` from the default branch.
