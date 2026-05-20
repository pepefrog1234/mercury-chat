#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WORKSPACE="$(cd "$ROOT/.." && pwd)"
QT_WIN_PREFIX="${QT_WIN_PREFIX:-$WORKSPACE/.qt-win/6.10.2/mingw_64}"
QT_HOST_PATH="${QT_HOST_PATH:-/opt/homebrew/Cellar/qtbase/6.10.2}"
MERCURY_ROOT="${MERCURY_ROOT:-$WORKSPACE/mercury}"
BUILD_DIR="${BUILD_DIR:-$ROOT/build-windows-x64}"
DIST_ROOT="${DIST_ROOT:-$WORKSPACE/dist/windows-x64}"

if [[ ! -d "$QT_HOST_PATH" ]] && command -v brew >/dev/null 2>&1; then
    QT_HOST_PATH="$(brew --prefix qtbase 2>/dev/null || brew --prefix qt 2>/dev/null || true)"
fi

for path in "$QT_WIN_PREFIX" "$QT_HOST_PATH" "$MERCURY_ROOT"; do
    if [[ ! -d "$path" ]]; then
        echo "Missing required path: $path" >&2
        exit 1
    fi
done

cmake -S "$ROOT" -B "$BUILD_DIR" -G "Unix Makefiles" \
    -DCMAKE_TOOLCHAIN_FILE="$ROOT/cmake/toolchains/windows-x64-mingw.cmake" \
    -DCMAKE_PREFIX_PATH="$QT_WIN_PREFIX" \
    -DQT_HOST_PATH="$QT_HOST_PATH" \
    -DCMAKE_DISABLE_FIND_PACKAGE_PkgConfig=TRUE \
    -DMERCURYCHAT_USE_HAMLIB=ON \
    -DHAMLIB_INCLUDE_DIR="$MERCURY_ROOT/radio_io/hamlib-w64/include" \
    -DHAMLIB_LIBRARY="$MERCURY_ROOT/radio_io/hamlib-w64/lib/libhamlib.dll.a"
cmake --build "$BUILD_DIR" --parallel

version="$(sed -n '/^project(/,/^)/s/.*VERSION \([0-9][0-9.]*\).*/\1/p' "$ROOT/CMakeLists.txt" | head -1)"
chat_hash="$(git -C "$ROOT" rev-parse --short=8 HEAD 2>/dev/null || echo unknown)"
modem_hash="$(git -C "$MERCURY_ROOT" rev-parse --short=8 HEAD 2>/dev/null || echo unknown)"
package_name="mercury-chat-${version}-w64-${chat_hash}-modem-${modem_hash}"
package_dir="$DIST_ROOT/$package_name"
zip_path="$DIST_ROOT/$package_name.zip"

rm -rf "$package_dir" "$zip_path"
mkdir -p "$package_dir" "$package_dir/plugins"

copy_file() {
    local src="$1"
    local dst="${2:-$package_dir/$(basename "$src")}"
    if [[ ! -f "$src" ]]; then
        echo "Missing file: $src" >&2
        exit 1
    fi
    mkdir -p "$(dirname "$dst")"
    cp "$src" "$dst"
}

copy_plugin() {
    local rel="$1"
    copy_file "$QT_WIN_PREFIX/plugins/$rel" "$package_dir/plugins/$rel"
}

copy_file "$BUILD_DIR/mercury-chat.exe"
copy_file "$MERCURY_ROOT/mercury.exe"
copy_file "$MERCURY_ROOT/mercury.ini.example"

for dll in \
    Qt6Core.dll Qt6Gui.dll Qt6Widgets.dll Qt6Network.dll Qt6Sql.dll \
    Qt6Multimedia.dll Qt6OpenGL.dll Qt6Svg.dll Qt6Xml.dll \
    libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll \
    d3dcompiler_47.dll opengl32sw.dll \
    avcodec-61.dll avformat-61.dll avutil-59.dll swresample-5.dll swscale-8.dll
do
    copy_file "$QT_WIN_PREFIX/bin/$dll"
done

for dll in libhamlib-4.dll libusb-1.0.dll; do
    copy_file "$MERCURY_ROOT/radio_io/hamlib-w64/bin/$dll"
done

copy_plugin "platforms/qwindows.dll"
copy_plugin "styles/qmodernwindowsstyle.dll"
copy_plugin "sqldrivers/qsqlite.dll"
copy_plugin "multimedia/ffmpegmediaplugin.dll"
copy_plugin "multimedia/windowsmediaplugin.dll"
copy_plugin "tls/qcertonlybackend.dll"
copy_plugin "tls/qschannelbackend.dll"
copy_plugin "networkinformation/qnetworklistmanager.dll"
copy_plugin "imageformats/qgif.dll"
copy_plugin "imageformats/qico.dll"
copy_plugin "imageformats/qjpeg.dll"
copy_plugin "imageformats/qsvg.dll"
copy_plugin "iconengines/qsvgicon.dll"

cat > "$package_dir/qt.conf" <<'EOF'
[Paths]
Plugins = plugins
EOF

cat > "$package_dir/README-WINDOWS.txt" <<EOF
Mercury Chat Windows x64 portable package

Run:
  mercury-chat.exe

This package includes the forked Mercury modem as mercury.exe.

Build metadata:
  Mercury Chat commit: $chat_hash
  Mercury modem commit: $modem_hash
  Qt target: 6.10.2 win64_mingw

Notes:
  - Settings and SQLite chat logs are stored by the application at runtime.
  - Keep mercury.exe in the same folder as mercury-chat.exe unless you set a
    custom modem executable path in the GUI.
EOF

(cd "$DIST_ROOT" && zip -9r "$zip_path" "$package_name")
echo "Created $zip_path"
