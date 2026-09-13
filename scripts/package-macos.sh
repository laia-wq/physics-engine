#!/bin/bash

set -euo pipefail

SCRIPT_DIRECTORY="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIRECTORY="$(cd "$SCRIPT_DIRECTORY/.." && pwd)"
BUILD_DIRECTORY="$PROJECT_DIRECTORY/build-release"
DIST_DIRECTORY="$PROJECT_DIRECTORY/dist"
APP_DIRECTORY="$DIST_DIRECTORY/Particle Physics Laboratory.app"
CONTENTS_DIRECTORY="$APP_DIRECTORY/Contents"
EXECUTABLE_DIRECTORY="$CONTENTS_DIRECTORY/MacOS"
FRAMEWORKS_DIRECTORY="$CONTENTS_DIRECTORY/Frameworks"
ARCHIVE_PATH="$DIST_DIRECTORY/Particle-Physics-Laboratory-macOS-arm64.zip"

if [[ "$(uname -m)" != "arm64" ]]; then
    echo "This release script currently packages the verified arm64 build only."
    exit 1
fi

SFML_DIRECTORY="$(brew --prefix sfml)/lib"
FREETYPE_DIRECTORY="$(brew --prefix freetype)/lib"
LIBPNG_DIRECTORY="$(brew --prefix libpng)/lib"

cmake -S "$PROJECT_DIRECTORY" -B "$BUILD_DIRECTORY" \
    -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build "$BUILD_DIRECTORY" -j4
ctest --test-dir "$BUILD_DIRECTORY" --output-on-failure

cmake -E rm -rf "$APP_DIRECTORY"
cmake -E make_directory "$EXECUTABLE_DIRECTORY" "$FRAMEWORKS_DIRECTORY"
cmake -E copy "$PROJECT_DIRECTORY/packaging/Info.plist" "$CONTENTS_DIRECTORY/Info.plist"
cmake -E copy "$BUILD_DIRECTORY/physics-engine" "$EXECUTABLE_DIRECTORY/physics-engine"
cmake -E copy "$SFML_DIRECTORY/libsfml-graphics.3.0.dylib" "$FRAMEWORKS_DIRECTORY"
cmake -E copy "$SFML_DIRECTORY/libsfml-window.3.0.dylib" "$FRAMEWORKS_DIRECTORY"
cmake -E copy "$SFML_DIRECTORY/libsfml-system.3.0.dylib" "$FRAMEWORKS_DIRECTORY"
cmake -E copy "$FREETYPE_DIRECTORY/libfreetype.6.dylib" "$FRAMEWORKS_DIRECTORY"
cmake -E copy "$LIBPNG_DIRECTORY/libpng16.16.dylib" "$FRAMEWORKS_DIRECTORY"

EXECUTABLE="$EXECUTABLE_DIRECTORY/physics-engine"
install_name_tool -add_rpath "@executable_path/../Frameworks" "$EXECUTABLE"
for LIBRARY in graphics window system; do
    install_name_tool -change \
        "$SFML_DIRECTORY/libsfml-$LIBRARY.3.0.dylib" \
        "@rpath/libsfml-$LIBRARY.3.0.dylib" \
        "$EXECUTABLE"
done

GRAPHICS_LIBRARY="$FRAMEWORKS_DIRECTORY/libsfml-graphics.3.0.dylib"
WINDOW_LIBRARY="$FRAMEWORKS_DIRECTORY/libsfml-window.3.0.dylib"
SYSTEM_LIBRARY="$FRAMEWORKS_DIRECTORY/libsfml-system.3.0.dylib"
FREETYPE_LIBRARY="$FRAMEWORKS_DIRECTORY/libfreetype.6.dylib"
PNG_LIBRARY="$FRAMEWORKS_DIRECTORY/libpng16.16.dylib"

install_name_tool -id "@rpath/libsfml-graphics.3.0.dylib" "$GRAPHICS_LIBRARY"
install_name_tool -id "@rpath/libsfml-window.3.0.dylib" "$WINDOW_LIBRARY"
install_name_tool -id "@rpath/libsfml-system.3.0.dylib" "$SYSTEM_LIBRARY"
install_name_tool -id "@rpath/libfreetype.6.dylib" "$FREETYPE_LIBRARY"
install_name_tool -id "@rpath/libpng16.16.dylib" "$PNG_LIBRARY"

install_name_tool -change \
    "$FREETYPE_DIRECTORY/libfreetype.6.dylib" \
    "@rpath/libfreetype.6.dylib" "$GRAPHICS_LIBRARY"
install_name_tool -change \
    "$LIBPNG_DIRECTORY/libpng16.16.dylib" \
    "@rpath/libpng16.16.dylib" "$FREETYPE_LIBRARY"

codesign --force --deep --sign - "$APP_DIRECTORY"
cmake -E rm -f "$ARCHIVE_PATH"
ditto -c -k --norsrc --keepParent "$APP_DIRECTORY" "$ARCHIVE_PATH"

echo "Created: $APP_DIRECTORY"
echo "Created: $ARCHIVE_PATH"
