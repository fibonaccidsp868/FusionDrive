#!/usr/bin/env bash
# FusionDrive - one-command build + install + installer (.pkg / .dmg) for macOS.
# Usage:  ./build_mac.sh            build, install for current user, create dist/*.pkg and *.dmg
#         ./build_mac.sh --no-install   build and package only
set -euo pipefail
cd "$(dirname "$0")"
NAME="FusionDrive"; VERSION="1.0.0"; ID="com.fusiondrive.audio"
INSTALL=1; [[ "${1:-}" == "--no-install" ]] && INSTALL=0

[[ "$(uname)" == "Darwin" ]] || { echo "This script must be run on macOS."; exit 1; }
if ! xcode-select -p >/dev/null 2>&1; then
  echo "Installing Xcode Command Line Tools - re-run this script when that finishes."; xcode-select --install || true; exit 1
fi
if ! command -v cmake >/dev/null 2>&1; then
  if command -v brew >/dev/null 2>&1; then brew install cmake
  else echo "CMake missing. Install Homebrew (https://brew.sh) then run: brew install cmake"; exit 1; fi
fi

echo "==> Configuring (first run downloads JUCE, needs internet)"
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
echo "==> Building universal (arm64 + x86_64) AU, VST3, Standalone"
cmake --build build --config Release -j "$(sysctl -n hw.ncpu)"

AU="$(find build -type d -name "$NAME.component" | head -1)"
VST="$(find build -type d -name "$NAME.vst3" | head -1)"
APP="$(find build -type d -name "$NAME.app" | head -1)"
[[ -n "$AU" && -n "$VST" ]] || { echo "Build products not found."; exit 1; }

echo "==> Ad-hoc code signing"
for b in "$AU" "$VST" "$APP"; do
  if [[ -n "$b" ]]; then codesign --force --deep -s - "$b"; fi
done

if [[ $INSTALL -eq 1 ]]; then
  echo "==> Installing for current user"
  mkdir -p "$HOME/Library/Audio/Plug-Ins/Components" "$HOME/Library/Audio/Plug-Ins/VST3"
  rm -rf "$HOME/Library/Audio/Plug-Ins/Components/$NAME.component" "$HOME/Library/Audio/Plug-Ins/VST3/$NAME.vst3"
  cp -R "$AU"  "$HOME/Library/Audio/Plug-Ins/Components/"
  cp -R "$VST" "$HOME/Library/Audio/Plug-Ins/VST3/"
  xattr -dr com.apple.quarantine "$HOME/Library/Audio/Plug-Ins/Components/$NAME.component" "$HOME/Library/Audio/Plug-Ins/VST3/$NAME.vst3" 2>/dev/null || true
  killall -9 AudioComponentRegistrar 2>/dev/null || true
  echo "==> Validating AU (auval)"
  auval -v aufx Fdrv Fdau 2>&1 | tail -n 12 || true
fi

echo "==> Building installer package"
rm -rf dist stage; mkdir -p dist stage/Components stage/VST3
cp -R "$AU" stage/Components/; cp -R "$VST" stage/VST3/
pkgbuild --root stage --identifier "$ID" --version "$VERSION" --install-location "/Library/Audio/Plug-Ins" "dist/$NAME-$VERSION.pkg"
mkdir -p dist/dmg && cp "dist/$NAME-$VERSION.pkg" dist/dmg/
cat > dist/dmg/READ_ME_FIRST.txt <<TXT
Double-click $NAME-$VERSION.pkg to install the AU and VST3.
Unsigned build: if macOS blocks it, right-click the .pkg > Open, or System Settings > Privacy & Security > Open Anyway.
Then restart your DAW and rescan plugins (Ableton: Settings > Plug-ins > Rescan).
TXT
hdiutil create -volname "$NAME $VERSION" -srcfolder dist/dmg -ov -format UDZO "dist/$NAME-$VERSION.dmg" >/dev/null
rm -rf dist/dmg stage
echo; echo "Done. Installer: $(pwd)/dist/$NAME-$VERSION.dmg"
if [[ $INSTALL -eq 1 ]]; then echo "Plugins are also installed for your user. Restart your DAW and rescan."; fi
exit 0
