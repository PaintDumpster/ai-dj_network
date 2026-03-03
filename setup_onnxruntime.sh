#!/bin/bash
# ONNX Runtime C++ Setup Script for Ubuntu 24.04
# This downloads and installs ONNX Runtime C++ libraries (NOT Python)

set -e

# ONNX Runtime version - use latest stable
ONNX_VERSION="1.24.2"
INSTALL_DIR="/opt/onnxruntime"

echo "================================================"
echo "ONNX Runtime C++ Setup"
echo "Version: ${ONNX_VERSION}"
echo "Install Directory: ${INSTALL_DIR}"
echo "================================================"

# Detect architecture
ARCH=$(uname -m)
if [ "$ARCH" = "x86_64" ]; then
    ARCH_NAME="x64"
elif [ "$ARCH" = "aarch64" ]; then
    ARCH_NAME="aarch64"
else
    echo "Unsupported architecture: $ARCH"
    exit 1
fi

PACKAGE_NAME="onnxruntime-linux-${ARCH_NAME}-${ONNX_VERSION}"
DOWNLOAD_URL="https://github.com/microsoft/onnxruntime/releases/download/v${ONNX_VERSION}/${PACKAGE_NAME}.tgz"

echo "Detected architecture: $ARCH -> $ARCH_NAME"
echo ""

# Create temporary directory
TMP_DIR=$(mktemp -d)
cd "$TMP_DIR"

echo "Downloading ONNX Runtime..."
wget -q --show-progress "$DOWNLOAD_URL"

echo "Extracting archive..."
tar -xzf "${PACKAGE_NAME}.tgz"

echo "Installing to ${INSTALL_DIR}..."
sudo mkdir -p "$INSTALL_DIR"
sudo cp -r "${PACKAGE_NAME}"/* "$INSTALL_DIR/"

# Create symlinks for easy cmake finding
echo "Creating system symlinks..."
sudo ln -sf "${INSTALL_DIR}/include" /usr/local/include/onnxruntime
sudo ln -sf "${INSTALL_DIR}/lib/libonnxruntime.so" /usr/local/lib/libonnxruntime.so
sudo ln -sf "${INSTALL_DIR}/lib/libonnxruntime.so.${ONNX_VERSION}" /usr/local/lib/libonnxruntime.so.${ONNX_VERSION}

# Update library cache
echo "Updating library cache..."
sudo ldconfig

# Cleanup
cd ~
rm -rf "$TMP_DIR"

echo ""
echo "================================================"
echo "✓ ONNX Runtime C++ installed successfully!"
echo "================================================"
echo "Installation location: ${INSTALL_DIR}"
echo "Headers: /usr/local/include/onnxruntime"
echo "Library: /usr/local/lib/libonnxruntime.so"
echo ""
echo "You can now build your ROS2 package:"
echo "  cd <workspace>"
echo "  colcon build --packages-select cpp_pkg"
echo "================================================"
