# Installing ONNX Runtime for YAMNet Classification Node

## Quick Answer

**NO venv needed!** ONNX Runtime for C++ is a system library, not a Python package.

**Recommended version**: 1.24.2 (latest stable)

## Installation Steps

### Option 1: Automated Installation (Recommended)

Run the provided setup script from your workspace root:

```bash
cd <workspace>
./setup_onnxruntime.sh
```

This will:
- Download ONNX Runtime 1.24.2 C++ libraries
- Install to `/opt/onnxruntime`
- Create symlinks in `/usr/local/include` and `/usr/local/lib`
- Update library cache

### Option 2: Manual Installation

```bash
# Download prebuilt binaries (for x64)
cd /tmp
wget https://github.com/microsoft/onnxruntime/releases/download/v1.24.2/onnxruntime-linux-x64-1.24.2.tgz
tar -xzf onnxruntime-linux-x64-1.24.2.tgz

# Install system-wide
sudo mkdir -p /opt/onnxruntime
sudo cp -r onnxruntime-linux-x64-1.24.2/* /opt/onnxruntime/

# Create symlinks
sudo ln -sf /opt/onnxruntime/include /usr/local/include/onnxruntime
sudo ln -sf /opt/onnxruntime/lib/libonnxruntime.so /usr/local/lib/libonnxruntime.so
sudo ln -sf /opt/onnxruntime/lib/libonnxruntime.so.1.24.2 /usr/local/lib/libonnxruntime.so.1.24.2

# Update library cache
sudo ldconfig
```

## Build Your ROS2 Package

After installing ONNX Runtime:

```bash
cd <workspace>
colcon build --packages-select cpp_pkg
source install/setup.bash
```

The CMakeLists.txt will automatically detect ONNX Runtime and build the `yamnet_classification` node.

## Verify Installation

```bash
# Check if library is found
ldconfig -p | grep onnxruntime

# Should output something like:
# libonnxruntime.so.1.24.2 (libc6,x86-64) => /usr/local/lib/libonnxruntime.so.1.24.2
# libonnxruntime.so (libc6,x86-64) => /usr/local/lib/libonnxruntime.so
```

## Download YAMNet Model

You'll also need the YAMNet ONNX model file:

```bash
# Create models directory
mkdir -p <workspace>/models

# Download YAMNet ONNX model (if available) or convert from TensorFlow
# Option 1: Find pre-converted ONNX model online
# Option 2: Convert from TensorFlow Hub using tf2onnx (requires Python env)
```

## Why No Virtual Environment?

- **Python venv**: Only for Python packages installed via pip
- **C++ libraries**: Installed system-wide or linked via CMake
- ONNX Runtime has **both** Python and C++ APIs
- We're using the **C++ API** for ROS2 C++ nodes

Think of it like OpenCV or Boost - they're C++ libraries, not Python packages!

## Troubleshooting

### CMake can't find ONNX Runtime

Check these paths exist:
```bash
ls -la /usr/local/include/onnxruntime
ls -la /usr/local/lib/libonnxruntime.so
```

### Runtime error: "libonnxruntime.so not found"

```bash
sudo ldconfig
export LD_LIBRARY_PATH=/opt/onnxruntime/lib:$LD_LIBRARY_PATH
```

### Still issues?

Check CMake output during build:
```bash
colcon build --packages-select cpp_pkg --event-handlers console_direct+
```

Look for: "ONNX Runtime found" or "ONNX Runtime NOT found"
