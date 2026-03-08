# Using rosdep for Dependency Management

This project uses rosdep to manage Python dependencies for the web interface and ROS2 packages.

## What is rosdep?

rosdep is ROS's dependency management tool that:
- Automatically installs system dependencies
- Works across different platforms (Ubuntu, Debian, macOS)
- Integrates with package managers (apt, pip, brew)
- Ensures consistent dependency versions

## Initial Setup

### 1. Install rosdep

```bash
sudo apt install python3-rosdep
```

### 2. Initialize rosdep (First time only)

```bash
sudo rosdep init
rosdep update
```

### 3. Configure Custom Dependencies (Optional)

This project includes a `rosdep.yaml` file with custom dependency definitions for web framework packages. To use it system-wide:

```bash
# Add custom rosdep source
echo "yaml file://$(pwd)/rosdep.yaml" | sudo tee /etc/ros/rosdep/sources.list.d/50-ai-dj.list

# Update rosdep database
rosdep update
```

**OR** use the simpler workspace-only approach (recommended):

```bash
# No system configuration needed!
# Just run rosdep install from the workspace root
cd ~/iaac/ai4all/rosnetwork
rosdep install --from-paths src --ignore-src -y
```

## Installing Dependencies

### Install All Dependencies

From the workspace root:

```bash
cd ~/iaac/ai4all/rosnetwork
rosdep install --from-paths src --ignore-src -y
```

**What this does**:
- Scans all `package.xml` files in `src/`
- Resolves dependencies using rosdep database + custom `rosdep.yaml`
- Installs missing packages via apt or pip
- Skips packages that are already installed

### Install for Specific Package

```bash
rosdep install --from-paths src/py_pkg --ignore-src -y
```

### Check What Would Be Installed (Dry Run)

```bash
rosdep install --from-paths src --ignore-src -y --simulate
```

## Declared Dependencies

### py_pkg (Python Package)

**File**: `src/py_pkg/package.xml`

**Dependencies**:
- `rclpy` - ROS2 Python client library
- `std_msgs` - Standard message types
- `std_srvs` - Standard service types
- `python3-fastapi` - Web framework
- `python3-uvicorn` - ASGI server
- `python3-websockets` - WebSocket support

### cpp_pkg (C++ Package)

**File**: `src/cpp_pkg/package.xml`

**Dependencies**:
- `rclcpp` - ROS2 C++ client library
- `std_msgs` - Standard message types
- `std_srvs` - Standard service types
- ONNX Runtime (installed separately via `setup_onnxruntime.sh`)

## Custom rosdep.yaml

**File**: `rosdep.yaml`

This file defines how to install web framework dependencies that may not be in the standard rosdep database:

```yaml
python3-fastapi:
  ubuntu:
    pip:
      packages: [fastapi>=0.104.0]

python3-uvicorn:
  ubuntu:
    pip:
      packages: ['uvicorn[standard]>=0.24.0']

python3-websockets:
  ubuntu:
    focal: [python3-websockets]    # apt package
    jammy: [python3-websockets]    # apt package
    pip:
      packages: [websockets>=12.0] # fallback to pip
```

## Troubleshooting

### "Cannot locate rosdep definition"

If you see errors like:
```
ERROR: the following packages/stacks could not have their rosdep keys resolved
to system dependencies:
py_pkg: Cannot locate rosdep definition for [python3-fastapi]
```

**Solution**: Use the custom rosdep.yaml

```bash
# Option 1: Add to system rosdep sources (requires sudo)
echo "yaml file://$(pwd)/rosdep.yaml" | sudo tee /etc/ros/rosdep/sources.list.d/50-ai-dj.list
rosdep update

# Option 2: Set environment variable (workspace-only)
export ROSDEP_SOURCE_PATH=$(pwd)/rosdep.yaml
rosdep install --from-paths src --ignore-src -y

# Option 3: Install manually with pip
pip3 install fastapi 'uvicorn[standard]' websockets
```

### "python3-websockets not found"

The package name might vary by Ubuntu version:

```bash
# Check available version
apt-cache search python3-websockets

# Install manually if needed
sudo apt install python3-websockets
# OR
pip3 install websockets
```

### rosdep install fails with pip errors

**Cause**: pip packages specified in rosdep.yaml

**Solution**: Ensure pip is up to date

```bash
python3 -m pip install --upgrade pip
```

### Permission denied during pip install

rosdep may try to install pip packages system-wide.

**Solution**: Use user install or virtual environment

```bash
# Install to user directory
pip3 install --user fastapi uvicorn websockets

# OR use virtual environment
python3 -m venv ~/ros_venv
source ~/ros_venv/bin/activate
pip install fastapi uvicorn websockets
```

## Verifying Installation

### Check ROS Dependencies

```bash
ros2 pkg list | grep -E "(std_msgs|std_srvs|rclpy|rclcpp)"
```

### Check Python Packages

```bash
python3 -c "import fastapi; print(f'FastAPI: {fastapi.__version__}')"
python3 -c "import uvicorn; print(f'Uvicorn: {uvicorn.__version__}')"
python3 -c "import websockets; print(f'WebSockets: {websockets.__version__}')"
```

### List All Installed Dependencies

```bash
rosdep check --from-paths src --ignore-src
```

## Best Practices

### 1. Always Use rosdep for New Systems

When setting up on a new machine:
```bash
cd ~/iaac/ai4all/rosnetwork
rosdep install --from-paths src --ignore-src -y
```

### 2. Update rosdep After Pulling Changes

If `package.xml` files are updated:
```bash
git pull
rosdep install --from-paths src --ignore-src -y
```

### 3. Check Dependencies Before Building

```bash
rosdep check --from-paths src --ignore-src
```

### 4. Keep rosdep Database Updated

```bash
rosdep update
```

## Alternative: Manual Installation

If rosdep is not available or causes issues, install manually:

```bash
# ROS2 packages (should already be installed with ROS2)
sudo apt install \
    ros-humble-rclpy \
    ros-humble-rclcpp \
    ros-humble-std-msgs \
    ros-humble-std-srvs

# Python web framework
pip3 install fastapi 'uvicorn[standard]' websockets
```

## CI/CD Integration

For automated builds (GitHub Actions, GitLab CI):

```yaml
- name: Install dependencies
  run: |
    sudo apt update
    sudo apt install python3-rosdep
    sudo rosdep init || true
    rosdep update
    rosdep install --from-paths src --ignore-src -y
```

## References

- [rosdep documentation](http://wiki.ros.org/rosdep)
- [rosdep keys database](https://github.com/ros/rosdistro/tree/master/rosdep)
- [Creating custom rosdep rules](http://docs.ros.org/en/independent/api/rosdep/html/contributing_rules.html)

