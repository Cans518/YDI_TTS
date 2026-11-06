#!/usr/bin/env bash
set -euo pipefail
umask 022

repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${1:-$repo_root/build/macos}"
output_dir="${2:-$repo_root/dist}"
qt_root="${QT_ROOT_DIR:?Set QT_ROOT_DIR to the Qt installation}"
architecture="${YDI_ARCH:-$(uname -m)}"
version="$(python3 "$repo_root/scripts/release-metadata.py" version)"
case "$architecture" in arm64|x86_64) ;; *) echo 'Unsupported macOS architecture.' >&2; exit 1 ;; esac
mkdir -p "$build_dir" "$output_dir"
build_dir="$(cd "$build_dir" && pwd)"
output_dir="$(cd "$output_dir" && pwd)"

cmake --fresh -S "$repo_root" -B "$build_dir" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$qt_root" \
    -DCMAKE_OSX_ARCHITECTURES="$architecture" -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0 \
    -DYDI_SINGLE_FILE=OFF -DYDI_DEPLOY_RUNTIME=OFF -DBUILD_TESTING=ON
cmake --build "$build_dir" --parallel "${BUILD_JOBS:-2}"
export QT_QPA_PLATFORM=offscreen QT_FORCE_STDERR_LOGGING=1
export YDI_UI_CAPTURE_DIR="$build_dir/verification"
mkdir -p "$YDI_UI_CAPTURE_DIR"
ctest --test-dir "$build_dir" --output-on-failure --output-junit "$build_dir/tests.xml"

stage="$(mktemp -d "$build_dir/stage.XXXXXX")"
cmake --install "$build_dir" --prefix "$stage"
app="$stage/YDI_TTS.app"
"$qt_root/bin/macdeployqt" "$app" -always-overwrite -codesign=- -verbose=2
test -f "$app/Contents/Resources/qt.conf"
python3 "$repo_root/scripts/verify-bundle.py" macos "$app" "$qt_root" "$build_dir"
codesign --force --deep --sign - "$app"
codesign --verify --deep --strict --verbose=2 "$app"
lipo "$app/Contents/MacOS/YDI_TTS" -verify_arch "$architecture"
test -f "$app/Contents/PlugIns/platforms/libqcocoa.dylib" || \
    test -f "$app/Contents/plugins/platforms/libqcocoa.dylib"

smoke_dir="$(mktemp -d "$build_dir/独立启动.XXXXXX")"
ditto "$app" "$smoke_dir/YDI_TTS.app"
env -u QT_PLUGIN_PATH -u QT_QPA_PLATFORM_PLUGIN_PATH -u DYLD_LIBRARY_PATH \
    -u DYLD_FRAMEWORK_PATH QT_QPA_PLATFORM=cocoa \
    "$smoke_dir/YDI_TTS.app/Contents/MacOS/YDI_TTS" --smoke-test

dmg_root="$(mktemp -d "$build_dir/dmg.XXXXXX")"
chmod 755 "$dmg_root"
ditto "$app" "$dmg_root/YDI_TTS.app"
ln -s /Applications "$dmg_root/Applications"
cp "$repo_root/docs/macos-distribution.txt" "$dmg_root/安装说明.txt"
artifact="$output_dir/YDI_TTS-$version-macos-$architecture-unnotarized.dmg"
hdiutil create -volname "YDI TTS $version" -srcfolder "$dmg_root" -format UDZO -ov "$artifact"
hdiutil verify "$artifact"
python3 "$repo_root/scripts/release-metadata.py" manifest "$output_dir"
echo "Unnotarized DMG verified: $artifact"
