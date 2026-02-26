#!/bin/bash

# Define paths
DIST_DIR="dist"
BUILD_DIR="build"
EXE_NAME="trunk-recorder.exe"

# Create dist directory
mkdir -p "$DIST_DIR"

# Copy the main executable
if [ -f "$BUILD_DIR/$EXE_NAME" ]; then
    cp "$BUILD_DIR/$EXE_NAME" "$DIST_DIR/"
    echo "Copied $EXE_NAME"
else
    echo "Error: $EXE_NAME not found in $BUILD_DIR"
    exit 1
fi

# Find and copy dependencies using ldd
echo "Collecting dependencies..."
deps=$(ldd "$DIST_DIR/$EXE_NAME" | grep "mingw" | awk '{print $3}')

for dep in $deps; do
    if [ -f "$dep" ]; then
        cp "$dep" "$DIST_DIR/"
        echo "Copied dependency: $dep"
    else
        echo "Warning: Dependency $dep not found"
    fi
done

# Bundle External Tools (sox and fdkaac)
echo "Bundling external tools..."
EXTERNAL_TOOLS=("sox.exe" "fdkaac.exe")
MINGW_BIN="/mingw64/bin"

for tool in "${EXTERNAL_TOOLS[@]}"; do
    TOOL_PATH="$MINGW_BIN/$tool"
    if [ -f "$TOOL_PATH" ]; then
        cp "$TOOL_PATH" "$DIST_DIR/"
        echo "Bundled tool: $tool"

        # Also copy dependencies for the tool
        tool_deps=$(ldd "$TOOL_PATH" | grep "mingw" | awk '{print $3}')
        for tdep in $tool_deps; do
            if [ ! -f "$DIST_DIR/$(basename "$tdep")" ]; then
                if [ -f "$tdep" ]; then
                    cp "$tdep" "$DIST_DIR/"
                    echo "Copied dependency for $tool: $tdep"
                fi
            fi
        done
    else
        echo "Warning: External tool $tool not found in $MINGW_BIN"
    fi
done

# Create a zip archive
zip -r "trunk-recorder-windows.zip" "$DIST_DIR"
echo "Created trunk-recorder-windows.zip"
