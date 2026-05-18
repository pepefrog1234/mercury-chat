#!/usr/bin/env sh
set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
APP=${MERCURYCHAT_APP:-"$ROOT_DIR/build/mercury-chat"}
SETTINGS=${MERCURYCHAT_B_SETTINGS:-"$ROOT_DIR/profiles/b.ini"}

if [ ! -x "$APP" ]; then
    printf 'mercury-chat executable not found: %s\nRun: cmake --build "%s/build"\n' "$APP" "$ROOT_DIR" >&2
    exit 1
fi

exec "$APP" --profile B --settings-file "$SETTINGS"
