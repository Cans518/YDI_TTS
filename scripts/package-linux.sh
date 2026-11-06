#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${1:-$repo_root/build/linux}"
output_dir="${2:-$repo_root/dist}"
qt_root="${QT_ROOT_DIR:?Set QT_ROOT_DIR to the Qt installation}"
jobs="${BUILD_JOBS:-2}"
version="$(python3 "$repo_root/scripts/release-metadata.py" version)"
[[ "$(uname -m)" == x86_64 ]] || { echo 'This AppImage recipe targets x86_64.' >&2; exit 1; }
mkdir -p "$build_dir" "$output_dir"
build_dir="$(cd "$build_dir" && pwd)"
output_dir="$(cd "$output_dir" && pwd)"

cmake --fresh -S "$repo_root" -B "$build_dir" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$qt_root" \
    -DYDI_SINGLE_FILE=OFF -DYDI_DEPLOY_RUNTIME=OFF -DBUILD_TESTING=ON
cmake --build "$build_dir" --parallel "$jobs"
export QT_QPA_PLATFORM=offscreen QT_FORCE_STDERR_LOGGING=1
export YDI_UI_CAPTURE_DIR="$build_dir/verification"
mkdir -p "$YDI_UI_CAPTURE_DIR"
ctest --test-dir "$build_dir" --output-on-failure --output-junit "$build_dir/tests.xml"

appdir="$(mktemp -d "$build_dir/AppDir.XXXXXX")"
DESTDIR="$appdir" cmake --install "$build_dir" --prefix /usr
tools_dir="$build_dir/tools"
python3 "$repo_root/scripts/fetch-appimage-tools.py" "$tools_dir"
export QMAKE="$qt_root/bin/qmake"
export APPIMAGE_EXTRACT_AND_RUN=1
export EXTRA_QT_MODULES='svg;waylandclient'
platforms=(libqoffscreen.so)
for plugin in "$qt_root"/plugins/platforms/libqwayland*.so; do
    [[ -f "$plugin" ]] && platforms+=("$(basename "$plugin")")
done
export EXTRA_PLATFORM_PLUGINS="$(IFS=';'; echo "${platforms[*]}")"
export OUTPUT="$output_dir/YDI_TTS-$version-linux-x86_64.AppImage"
unset QT_QPA_PLATFORM
"$tools_dir/linuxdeploy-x86_64.AppImage" --appdir "$appdir" \
    --executable "$appdir/usr/bin/YDI_TTS" \
    --desktop-file "$repo_root/packaging/linux/cn.ydich.tts.desktop" \
    --icon-file "$repo_root/resources/icons/cn.ydich.tts.png" \
    --plugin qt --output appimage
test -s "$OUTPUT"
chmod +x "$OUTPUT"
test -f "$appdir/usr/bin/qt.conf"
python3 "$repo_root/scripts/verify-bundle.py" linux "$appdir" "$qt_root" "$build_dir"

# Run the distributable with the development Qt environment removed.
env -u QT_PLUGIN_PATH -u QT_QPA_PLATFORM_PLUGIN_PATH -u LD_LIBRARY_PATH \
    QT_QPA_PLATFORM=offscreen APPIMAGE_EXTRACT_AND_RUN=1 "$OUTPUT" --smoke-test
xvfb-run -a env -u QT_PLUGIN_PATH -u QT_QPA_PLATFORM_PLUGIN_PATH -u LD_LIBRARY_PATH \
    QT_QPA_PLATFORM=xcb APPIMAGE_EXTRACT_AND_RUN=1 "$OUTPUT" --smoke-test

if command -v weston >/dev/null 2>&1; then
    wayland_runtime="$(mktemp -d "$build_dir/wayland.XXXXXX")"
    chmod 700 "$wayland_runtime"
    XDG_RUNTIME_DIR="$wayland_runtime" weston --backend=headless-backend.so \
        --use-pixman --socket=ydi-tts-test --idle-time=0 --log="$build_dir/weston.log" &
    weston_pid=$!
    trap 'kill "$weston_pid" 2>/dev/null || true' EXIT
    for attempt in $(seq 1 50); do
        [[ -S "$wayland_runtime/ydi-tts-test" ]] && break
        sleep 0.1
    done
    test -S "$wayland_runtime/ydi-tts-test"
    env -u QT_PLUGIN_PATH -u QT_QPA_PLATFORM_PLUGIN_PATH -u LD_LIBRARY_PATH \
        XDG_RUNTIME_DIR="$wayland_runtime" WAYLAND_DISPLAY=ydi-tts-test \
        QT_QPA_PLATFORM=wayland APPIMAGE_EXTRACT_AND_RUN=1 "$OUTPUT" --smoke-test
    kill "$weston_pid"
    wait "$weston_pid" || true
    trap - EXIT
fi

python3 "$repo_root/scripts/release-metadata.py" manifest "$output_dir"
echo "AppImage verified: $OUTPUT"
