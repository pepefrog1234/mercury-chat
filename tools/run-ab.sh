#!/usr/bin/env sh
set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
APP=${MERCURYCHAT_APP:-"$ROOT_DIR/build/mercury-chat"}

if [ ! -x "$APP" ]; then
    printf 'mercury-chat executable not found: %s\nRun: cmake --build "%s/build"\n' "$APP" "$ROOT_DIR" >&2
    exit 1
fi

"$APP" --profile A --settings-file "$ROOT_DIR/profiles/a.ini" &
pid_a=$!
"$APP" --profile B --settings-file "$ROOT_DIR/profiles/b.ini" &
pid_b=$!

wait "$pid_a" "$pid_b"
