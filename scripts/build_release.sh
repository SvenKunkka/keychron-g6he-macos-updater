#!/bin/zsh

set -euo pipefail

PROJECT_DIR="${0:A:h:h}"
VERSION="${VERSION:-1.0.0}"
APP_NAME="G6 HE Firmware Updater"
BUILD_DIR="$PROJECT_DIR/build-release"
DIST_DIR="$PROJECT_DIR/dist"
APP_PATH="$BUILD_DIR/$APP_NAME.app"
FIRMWARE_NAME="G6HE_v1.0.0+84_202609101503.signed.bin"
VENV_DIR="$PROJECT_DIR/.build-venv"

if [[ ! -f "$PROJECT_DIR/g6he_mac_tool.py" || ! -f "$PROJECT_DIR/firmware/$FIRMWARE_NAME" ]]; then
  echo "Project files or bundled firmware are missing." >&2
  exit 1
fi

if [[ ! -x "$VENV_DIR/bin/pyinstaller" ]]; then
  python3 -m venv "$VENV_DIR"
  "$VENV_DIR/bin/python" -m pip install -r "$PROJECT_DIR/requirements-build.txt"
fi

HIDAPI_PREFIX="$(brew --prefix hidapi)"
if [[ ! -f "$HIDAPI_PREFIX/lib/libhidapi.dylib" ]]; then
  echo "hidapi is required: brew install hidapi" >&2
  exit 1
fi

if [[ -d "$BUILD_DIR" ]]; then
  rm -rf "$BUILD_DIR"
fi
mkdir -p "$BUILD_DIR/pyinstaller" "$DIST_DIR"

"$VENV_DIR/bin/pyinstaller" \
  --noconfirm \
  --clean \
  --onefile \
  --name g6he-updater-cli \
  --distpath "$BUILD_DIR/pyinstaller" \
  --workpath "$BUILD_DIR/pyinstaller-work" \
  --specpath "$BUILD_DIR" \
  --add-binary "$HIDAPI_PREFIX/lib/libhidapi.dylib:." \
  "$PROJECT_DIR/g6he_mac_tool.py"

mkdir -p "$APP_PATH/Contents/MacOS" "$APP_PATH/Contents/Resources/Firmware" "$APP_PATH/Contents/Resources/Licenses"
clang -fobjc-arc -fblocks -mmacosx-version-min=13.0 -framework Cocoa \
  -framework UniformTypeIdentifiers \
  -o "$APP_PATH/Contents/MacOS/G6HEFirmwareUpdater" \
  "$PROJECT_DIR/macos-app/Sources/main.m"
cp "$PROJECT_DIR/macos-app/Info.plist" "$APP_PATH/Contents/Info.plist"
cp "$BUILD_DIR/pyinstaller/g6he-updater-cli" "$APP_PATH/Contents/MacOS/g6he-updater-cli"
cp "$PROJECT_DIR/firmware/$FIRMWARE_NAME" "$APP_PATH/Contents/Resources/Firmware/$FIRMWARE_NAME"
cp "$HIDAPI_PREFIX/LICENSE-bsd.txt" "$APP_PATH/Contents/Resources/Licenses/hidapi-BSD.txt"
cp "$PROJECT_DIR/LICENSE" "$APP_PATH/Contents/Resources/Licenses/application-MIT.txt"

ICON_WORK="$BUILD_DIR/icon"
mkdir -p "$ICON_WORK" "$ICON_WORK/AppIcon.iconset"
qlmanage -t -s 1024 -o "$ICON_WORK" "$PROJECT_DIR/macos-app/Resources/AppIcon.svg" >/dev/null 2>&1
SOURCE_PNG="$ICON_WORK/AppIcon.svg.png"
if [[ -f "$SOURCE_PNG" ]]; then
  for size in 16 32 128 256 512; do
    sips -z "$size" "$size" "$SOURCE_PNG" --out "$ICON_WORK/AppIcon.iconset/icon_${size}x${size}.png" >/dev/null
    double=$((size * 2))
    sips -z "$double" "$double" "$SOURCE_PNG" --out "$ICON_WORK/AppIcon.iconset/icon_${size}x${size}@2x.png" >/dev/null
  done
  iconutil -c icns "$ICON_WORK/AppIcon.iconset" -o "$APP_PATH/Contents/Resources/AppIcon.icns"
  /usr/libexec/PlistBuddy -c "Add :CFBundleIconFile string AppIcon" "$APP_PATH/Contents/Info.plist"
fi

codesign --force --deep --sign - "$APP_PATH"
codesign --verify --deep --strict --verbose=2 "$APP_PATH"

ZIP_PATH="$DIST_DIR/G6HE-Firmware-Updater-v$VERSION-macOS-arm64.zip"
ditto -c -k --sequesterRsrc --keepParent "$APP_PATH" "$ZIP_PATH"

DMG_STAGE="$BUILD_DIR/dmg"
mkdir -p "$DMG_STAGE"
cp -R "$APP_PATH" "$DMG_STAGE/"
ln -s /Applications "$DMG_STAGE/Applications"
DMG_PATH="$DIST_DIR/G6HE-Firmware-Updater-v$VERSION-macOS-arm64.dmg"
hdiutil create -ov -volname "G6 HE Firmware Updater" -srcfolder "$DMG_STAGE" -format UDZO "$DMG_PATH"

cp "$PROJECT_DIR/firmware/$FIRMWARE_NAME" "$DIST_DIR/$FIRMWARE_NAME"
(
  cd "$DIST_DIR"
  shasum -a 256 \
    "${DMG_PATH:t}" \
    "${ZIP_PATH:t}" \
    "$FIRMWARE_NAME" > SHA256SUMS.txt
)
echo "Built:"
echo "$APP_PATH"
echo "$DMG_PATH"
echo "$ZIP_PATH"
