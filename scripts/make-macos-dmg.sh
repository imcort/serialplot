#!/usr/bin/env bash
#
# Build SerialPlot for macOS and package it into a distributable .dmg.
#
# The resulting app bundle contains the Qt frameworks, plugins and the Qwt
# framework it needs, so the recipient does not have to install Qt or Qwt.
#
# Usage:
#   scripts/make-macos-dmg.sh
#
# Environment variables:
#   BUILD_DIR        Build directory (default: build-macos)
#   DIST_DIR         Output directory for the .dmg (default: dist)
#   CMAKE_OSX_ARCHITECTURES
#                    Target architecture, e.g. arm64 (default), x86_64 or
#                    "arm64;x86_64" for a universal build. A universal build
#                    requires a universal Qt installation.
#   MACOSX_DEPLOYMENT_TARGET
#                    Minimum macOS version (default: 14.0)
#   QT_PREFIX        Qt installation prefix (default: detected via qtpaths6)
#   QWT_PREFIX       Qwt installation prefix (default: Homebrew's qwt)
#   BUILD_QWT=1      Ignore QWT_PREFIX and let CMake download/build Qwt
#   CODESIGN_IDENTITY
#                    "Developer ID Application: ..." identity used for signing
#                    and notarization. When unset, the bundle is ad-hoc signed,
#                    which still requires the recipient to allow it in
#                    Gatekeeper (see the Read Me inside the disk image).
#   NOTARY_PROFILE   Optional keychain profile for `xcrun notarytool`. When
#                    set (with CODESIGN_IDENTITY), the .dmg is submitted for
#                    notarization and the ticket is stapled.
#   SKIP_BUILD=1     Reuse the existing app bundle instead of rebuilding
#   KEEP_STAGING=1   Keep the temporary staging directory for inspection
#
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT/build-macos}"
DIST_DIR="${DIST_DIR:-$ROOT/dist}"
ARCH="${CMAKE_OSX_ARCHITECTURES:-$(uname -m)}"
MIN_MACOS="${MACOSX_DEPLOYMENT_TARGET:-14.0}"

info()  { printf '\033[1;34m==>\033[0m %s\n' "$*"; }
warn()  { printf '\033[1;33mwarning:\033[0m %s\n' "$*" >&2; }
die()   { printf '\033[1;31merror:\033[0m %s\n' "$*" >&2; exit 1; }

[[ "$(uname -s)" == "Darwin" ]] || die "This script only runs on macOS."

for tool in cmake hdiutil codesign otool file; do
    command -v "$tool" >/dev/null 2>&1 || die "Required tool not found: $tool"
done

VERSION="$(sed -n 's/^project(serialplot VERSION \([0-9][0-9.]*\).*/\1/p' "$ROOT/CMakeLists.txt")"
[[ -n "$VERSION" ]] || die "Could not determine project version from CMakeLists.txt."

BUILD_DATE="$(date '+%Y-%m-%d')"
if [[ "$ARCH" == *";"* ]]; then
    ARCH_LABEL="universal"
else
    ARCH_LABEL="$ARCH"
fi
case "$ARCH_LABEL" in
    arm64)  ARCH_LABEL="arm64" ;;
    x86_64) ARCH_LABEL="x86_64" ;;
    universal) ARCH_LABEL="universal" ;;
esac

# ---------------------------------------------------------------- Qt / Qwt --
QT_PREFIX="${QT_PREFIX:-}"
if [[ -z "$QT_PREFIX" ]]; then
    if command -v qtpaths6 >/dev/null 2>&1; then
        QT_PREFIX="$(qtpaths6 --query QT_INSTALL_PREFIX)"
    elif command -v qtpaths >/dev/null 2>&1; then
        QT_PREFIX="$(qtpaths --query QT_INSTALL_PREFIX)"
    fi
fi
[[ -n "$QT_PREFIX" && -d "$QT_PREFIX" ]] || die \
    "Qt not found. Set QT_PREFIX to the Qt installation prefix."

MACDEPLOYQT=""
for candidate in "$QT_PREFIX/bin/macdeployqt" "$(command -v macdeployqt 2>/dev/null || true)"; do
    if [[ -n "$candidate" && -x "$candidate" ]]; then
        MACDEPLOYQT="$candidate"
        break
    fi
done
[[ -n "$MACDEPLOYQT" ]] || die "macdeployqt not found (it ships with Qt)."

# Homebrew ships the Qt base frameworks and the add-on modules with different
# deployment targets. A binary cannot run on an older macOS than any of the
# libraries it links against, so raise the target to the highest minimum among
# the modules used by SerialPlot instead of producing a bundle that fails to
# launch on the version advertised in Info.plist.
qt_frameworks=(QtCore QtGui QtWidgets QtNetwork QtSvg QtSerialPort QtBluetooth)
for fw in "${qt_frameworks[@]}"; do
    fw_binary="$QT_PREFIX/lib/$fw.framework/Versions/A/$fw"
    [[ -f "$fw_binary" ]] || continue
    fw_min="$(otool -l "$fw_binary" 2>/dev/null \
        | awk '/LC_BUILD_VERSION/{found=1} found && /minos/{print $2; exit}')"
    [[ -n "$fw_min" ]] || continue
    if [[ "$(printf '%s\n%s\n' "$MIN_MACOS" "$fw_min" | sort -V | tail -n 1)" == "$fw_min" \
          && "$fw_min" != "$MIN_MACOS" ]]; then
        warn "$fw requires macOS $fw_min; raising the deployment target from $MIN_MACOS to $fw_min."
        MIN_MACOS="$fw_min"
    fi
done

QWT_ARGS=()
if [[ "${BUILD_QWT:-0}" == "1" ]]; then
    QWT_ARGS=(-DBUILD_QWT=ON)
else
    QWT_INCLUDE_DIR=""
    QWT_LIBRARY=""
    for prefix in "${QWT_PREFIX:-}" /opt/homebrew/opt/qwt /usr/local/opt/qwt; do
        [[ -n "$prefix" && -d "$prefix" ]] || continue
        if [[ -f "$prefix/lib/qwt.framework/qwt" ]]; then
            if [[ -d "$prefix/lib/qwt.framework/Headers" ]]; then
                QWT_INCLUDE_DIR="$prefix/lib/qwt.framework/Headers"
            else
                QWT_INCLUDE_DIR="$(find "$prefix/lib/qwt.framework/Versions" \
                    -maxdepth 2 -type d -name Headers 2>/dev/null | head -n 1)"
            fi
            QWT_LIBRARY="$prefix/lib/qwt.framework/qwt"
            break
        elif [[ -f "$prefix/lib/libqwt.dylib" ]]; then
            QWT_INCLUDE_DIR="$prefix/include"
            QWT_LIBRARY="$prefix/lib/libqwt.dylib"
            break
        fi
    done
    if [[ -n "$QWT_INCLUDE_DIR" && -n "$QWT_LIBRARY" ]]; then
        QWT_ARGS=(-DBUILD_QWT=OFF
                  -DQWT_INCLUDE_DIR="$QWT_INCLUDE_DIR"
                  -DQWT_LIBRARY="$QWT_LIBRARY")
        info "Using Qwt: $QWT_LIBRARY"
    else
        warn "Qwt not found; letting CMake download and build it."
        QWT_ARGS=(-DBUILD_QWT=ON)
    fi
fi

# -------------------------------------------------------------------- Build --
APP="$BUILD_DIR/serialplot.app"

if [[ "${SKIP_BUILD:-0}" == "1" ]]; then
    info "SKIP_BUILD=1: reusing $APP"
    [[ -d "$APP" ]] || die "No existing app bundle at $APP."
else
    info "Configuring $BUILD_DIR (Release, $ARCH_LABEL, macOS $MIN_MACOS+)"
    cmake -S "$ROOT" -B "$BUILD_DIR" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_OSX_ARCHITECTURES="$ARCH" \
        -DCMAKE_OSX_DEPLOYMENT_TARGET="$MIN_MACOS" \
        -DCMAKE_PREFIX_PATH="$QT_PREFIX" \
        "${QWT_ARGS[@]}"

    info "Building"
    cmake --build "$BUILD_DIR" --parallel
fi

[[ -d "$APP" ]] || die "App bundle not found: $APP"
[[ -x "$APP/Contents/MacOS/serialplot" ]] || die "App executable missing in $APP."

# --------------------------------------------------------- Deploy Qt / Qwt --
# macdeployqt rewrites the bundle in place. Always deploy into a copy so the
# build tree keeps a pristine app; deploying over an already deployed bundle
# makes macdeployqt fail to resolve the @rpath references it created earlier.
if [[ -d "$APP/Contents/Frameworks" ]]; then
    warn "$APP is already deployed; rebuilding a pristine bundle."
    rm -rf "$APP"
    cmake --build "$BUILD_DIR" --parallel >/dev/null
fi

DEPLOY_DIR="$BUILD_DIR/dmg-app"
rm -rf "$DEPLOY_DIR"
mkdir -p "$DEPLOY_DIR"
APP_DEPLOYED="$DEPLOY_DIR/SerialPlot.app"
info "Copying the app bundle for deployment"
ditto "$APP" "$APP_DEPLOYED"

info "Deploying Qt, plugins and Qwt with macdeployqt"
if [[ -n "${CODESIGN_IDENTITY:-}" ]]; then
    "$MACDEPLOYQT" "$APP_DEPLOYED" -always-overwrite -verbose=1 \
        -sign-for-notarization="$CODESIGN_IDENTITY"
else
    "$MACDEPLOYQT" "$APP_DEPLOYED" -always-overwrite -verbose=1
    info "Ad-hoc signing the app bundle"
    codesign --force --deep --sign - "$APP_DEPLOYED"
fi

# Fail loudly if anything still points into a build/package-manager location:
# such a bundle works on this machine only and breaks on the recipient's Mac.
info "Checking for non-portable library references"
bad_refs=0
while IFS= read -r -d '' candidate; do
    if file -b "$candidate" 2>/dev/null | grep -q "Mach-O"; then
        # `otool -L` also prints the library's own install name (LC_ID_DYLIB);
        # that is metadata, not a dependency, so filter it out.
        self_id="$(otool -D "$candidate" 2>/dev/null | sed -n '2p')"
        refs="$(otool -L "$candidate" 2>/dev/null | tail -n +2 \
            | awk -v self="$self_id" '$1 != self {print}' \
            | grep -E "($QT_PREFIX|$ROOT|/opt/homebrew|/usr/local|/Users/)" || true)"
        if [[ -n "$refs" ]]; then
            printf '%s\n%s\n' "$candidate:" "$refs" >&2
            bad_refs=1
        fi
    fi
done < <(find "$APP_DEPLOYED/Contents" -type f -print0)
[[ "$bad_refs" == "0" ]] || die \
    "The bundle still references external Qt/Qwt/build paths (see above)."

codesign --verify --deep --strict "$APP_DEPLOYED" || die "Code signature verification failed."

# ------------------------------------------------------------------ Staging --
STAGE="$(mktemp -d "${TMPDIR:-/tmp}/serialplot-dmg.XXXXXX")"
cleanup() {
    if [[ "${KEEP_STAGING:-0}" == "1" ]]; then
        info "Keeping staging directory: $STAGE"
    else
        rm -rf "$STAGE"
    fi
}
trap cleanup EXIT

# The bundle is renamed for a nicer display name; CFBundleExecutable still
# points to Contents/MacOS/serialplot, so this is safe.
cp -R "$APP_DEPLOYED" "$STAGE/SerialPlot.app"
ln -s /Applications "$STAGE/Applications"

sed -e "s/@VERSION@/$VERSION/g" \
    -e "s/@ARCH@/$ARCH_LABEL/g" \
    -e "s/@MIN_MACOS@/$MIN_MACOS/g" \
    -e "s/@BUILD_DATE@/$BUILD_DATE/g" \
    "$ROOT/misc/macos/dmg-readme.txt" > "$STAGE/Read Me.txt"
cp "$ROOT/COPYING" "$STAGE/License.txt"

# Remove any resource forks/AppleDouble files that may confuse recipients.
find "$STAGE" -name "._*" -delete 2>/dev/null || true
rm -rf "$STAGE/.fseventsd" "$STAGE/.Spotlight-V100" 2>/dev/null || true

# --------------------------------------------------------------------- DMG --
mkdir -p "$DIST_DIR"
DMG="$DIST_DIR/SerialPlot-$VERSION-macos$MIN_MACOS-$ARCH_LABEL.dmg"
info "Creating $DMG"
hdiutil create \
    -volname "SerialPlot $VERSION" \
    -srcfolder "$STAGE" \
    -fs HFS+ \
    -format UDZO \
    -imagekey zlib-level=9 \
    -ov "$DMG" >/dev/null
hdiutil verify "$DMG" >/dev/null || die "Disk image verification failed."

if [[ -n "${NOTARY_PROFILE:-}" ]]; then
    [[ -n "${CODESIGN_IDENTITY:-}" ]] || die \
        "NOTARY_PROFILE requires CODESIGN_IDENTITY to be set."
    info "Submitting $DMG for notarization"
    xcrun notarytool submit "$DMG" --keychain-profile "$NOTARY_PROFILE" --wait
    xcrun stapler staple "$DMG"
elif [[ -z "${CODESIGN_IDENTITY:-}" ]]; then
    warn "The app is ad-hoc signed only. Recipients must allow it in"
    warn "System Settings > Privacy & Security on first launch (see Read Me)."
fi

info "Done: $DMG"
ls -lh "$DMG"
