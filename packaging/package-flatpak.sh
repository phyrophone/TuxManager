#!/bin/bash
################################################################################
# package-flatpak.sh - Build and package Tux Manager Flatpak
################################################################################

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/config"

PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
OUTPUT_DIR="$PROJECT_ROOT/packaging/output"
MANIFEST="$PROJECT_ROOT/packaging/flatpak/io.github.benapetr.TuxManager.yml"
APP_ID="io.github.benapetr.TuxManager"
BRANCH="master"
RUNTIME_REF="org.kde.Platform//6.7"
SDK_REF="org.kde.Sdk//6.7"
BUILD_DIR="$PROJECT_ROOT/packaging/.flatpak-build"
REPO_DIR="$PROJECT_ROOT/packaging/.flatpak-repo"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --version)
            APP_VERSION="$2"
            shift 2
            ;;
        --branch)
            BRANCH="$2"
            shift 2
            ;;
        *)
            echo "Unknown option: $1"
            echo "Usage: $0 [--version x.y.z] [--branch master|stable|beta|...]"
            exit 1
            ;;
    esac
done

echo "================================="
echo "Building Tux Manager Flatpak"
echo "================================="
echo "Version: $APP_VERSION"
echo "Branch: $BRANCH"
echo "Manifest: $MANIFEST"
echo ""

if [ ! -f "$MANIFEST" ]; then
    echo "Error: Flatpak manifest not found: $MANIFEST"
    exit 1
fi

for cmd in flatpak flatpak-builder; do
    if ! command -v "$cmd" >/dev/null 2>&1; then
        echo "Error: Missing required tool: $cmd"
        echo "Install dependencies first, for example:"
        echo "  sudo apt-get install flatpak flatpak-builder"
        exit 1
    fi
done

if ! flatpak remote-list --columns=name 2>/dev/null | grep -qx "flathub"; then
    echo "Error: Flatpak remote 'flathub' is not configured."
    echo "Run:"
    echo "  flatpak remote-add --if-not-exists flathub https://flathub.org/repo/flathub.flatpakrepo"
    exit 1
fi

missing_refs=()
for ref in "$RUNTIME_REF" "$SDK_REF"; do
    if ! flatpak info "$ref" >/dev/null 2>&1; then
        missing_refs+=("$ref")
    fi
done

if [ ${#missing_refs[@]} -gt 0 ]; then
    echo "Error: Missing Flatpak runtime/sdk refs:"
    for ref in "${missing_refs[@]}"; do
        echo "  - $ref"
    done
    echo ""
    echo "Install them with:"
    echo "  flatpak install flathub ${missing_refs[*]}"
    exit 1
fi

mkdir -p "$OUTPUT_DIR"
rm -rf "$BUILD_DIR" "$REPO_DIR"

echo "Step 1: Building Flatpak repo..."
flatpak-builder \
    --force-clean \
    --repo="$REPO_DIR" \
    "$BUILD_DIR" \
    "$MANIFEST"

echo ""
echo "Step 2: Creating bundle..."
ARCH="$(flatpak --default-arch)"
BUNDLE_NAME="${APP_NAME}-${APP_VERSION}-${ARCH}.flatpak"
BUNDLE_PATH="$OUTPUT_DIR/$BUNDLE_NAME"

flatpak build-bundle "$REPO_DIR" "$BUNDLE_PATH" "$APP_ID" "$BRANCH"

echo ""
echo "================================="
echo "Build complete!"
echo "================================="
echo "Bundle: $BUNDLE_PATH"
echo ""
echo "To install:"
echo "  flatpak install --user $BUNDLE_PATH"
echo ""
echo "To run:"
echo "  flatpak run $APP_ID"
echo ""
