#!/bin/bash
set -euo pipefail

SCRIPTDIR=$(realpath "$(dirname "${BASH_SOURCE[0]}")")
REPODIR=$(realpath "$SCRIPTDIR/..")

TEMPDIR=$(mktemp -d)
cleanup() {
    if [[ ! -z "$TEMPDIR" && -d "$TEMPDIR" ]]; then
        rm -rf "$TEMPDIR"
    fi
}
trap cleanup EXIT

cd "$REPODIR"

TAGS=( $(git tag -l --contains HEAD) )
NUM_TAGS="${#TAGS[@]}"
if [[ $NUM_TAGS -eq 0 ]]; then
    echo "error: no tag on current commit"
    exit 1
elif [[ $NUM_TAGS -gt 1 ]]; then
    echo "error: multiple tags on current commit"
    echo "info: found tags ( ${TAGS[@]} )"
    exit 1
fi

TAG="${TAGS[0]}"
if [[ ! $TAG =~ v[0-9]+.[0-9]+.[0-9]+ ]]; then
    echo "error: tag does not match version regex"
    echo "info: tag was $TAG"
    exit 1
fi

VERSION="${TAG##v}"
echo ">> creating release $VERSION"

echo "- cloning repo"
cd "$TEMPDIR"
git clone --recursive "$REPODIR" "wl-mirror-$VERSION"

echo "- removing unneeded files"
rm -rf "wl-mirror-$VERSION/.git"
rm -rf "wl-mirror-$VERSION/.gitignore"
rm -rf "wl-mirror-$VERSION/.gitmodules"
rm -rf "wl-mirror-$VERSION/proto/wayland-protocols/.git"
rm -rf "wl-mirror-$VERSION/proto/wlr-protocols/.git"
mkdir -p "wl-mirror-$VERSION/proto/wayland-protocols/.git"
mkdir -p "wl-mirror-$VERSION/proto/wlr-protocols/.git"

echo "- adding version file"
echo "$TAG" > "wl-mirror-$VERSION/version.txt"

echo "- creating archive"
tar caf "$REPODIR/wl-mirror-$VERSION.tar.gz" "wl-mirror-$VERSION/"

if [[ ! -z "${SIGKEY+z}" ]]; then
    echo "- signing archive"
    gpg --yes -u "$SIGKEY" -o "$REPODIR/wl-mirror-$VERSION.tar.gz.sig" --detach-sig "$REPODIR/wl-mirror-$VERSION.tar.gz"
else
    echo "- skipping signing archive (SIGKEY not set)"
fi

echo "- success"
