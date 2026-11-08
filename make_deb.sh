#!/bin/sh
# Build binary Debian packages with a snapshot version, e.g.
# 0.90~beta+20260914.144712.gff27a37 ("~beta" sorts before the final 0.90).
# The timestamp is the UTC commit time, so builds of the same commit get the
# same version on every platform. If tracked files have uncommitted changes,
# the build time is used instead and ".dirty" is appended, keeping such builds
# unique. debian/rules picks the version up from MHUXD_DEB_VERSION.
#
# Release builds: if HEAD carries the tag v<version> (version from
# configure.ac, e.g. v0.90) and the tree is clean, the packages get the plain
# version from debian/changelog, which must match configure.ac.
#
# No configure run needed: dpkg-buildpackage runs autoreconf and configure in
# the source tree itself, and cleans it again afterwards (-tc), so an
# out-of-tree build directory stays usable. The .deb files land in the parent
# directory of the source tree, as with plain dpkg-buildpackage.

set -e
cd "$(dirname "$0")"

upstream=$(sed -n 's/^AC_INIT(\[[^]]*\], *\[\([^]]*\)\].*/\1/p' configure.ac |
	sed -E 's/(alpha|beta|rc)/~\1/')
if [ -z "$upstream" ]; then
	echo "Can't find version in configure.ac" >&2
	exit 1
fi

release=$(sed -n 's/^AC_INIT(\[[^]]*\], *\[\([^]]*\)\].*/\1/p' configure.ac)
if git rev-parse -q --verify "refs/tags/v$release" >/dev/null 2>&1 &&
	[ "$(git rev-parse HEAD)" = "$(git rev-parse "v$release^{commit}")" ] &&
	git diff --quiet HEAD --; then
	changelog=$(dpkg-parsechangelog -S Version)
	if [ "$changelog" != "$release" ]; then
		echo "Tag v$release, but debian/changelog has version $changelog" >&2
		exit 1
	fi
	echo "Building Debian packages, release $release"
	dpkg-buildpackage -us -uc -b -tc
	ls -1 ../*_"$release"_*.deb
	exit 0
fi

stamp=$(date -u +%Y%m%d.%H%M%S)
suffix=
if rev=$(git rev-parse --short HEAD 2>/dev/null); then
	suffix=.g$rev
	if git diff --quiet HEAD --; then
		stamp=$(TZ=UTC0 git log -1 --date=format-local:%Y%m%d.%H%M%S --format=%cd)
	else
		suffix=$suffix.dirty
	fi
fi
version=$upstream+$stamp$suffix

echo "Building Debian packages, version $version"
MHUXD_DEB_VERSION=$version dpkg-buildpackage -us -uc -b -tc
ls -1 ../*_"$version"_*.deb
