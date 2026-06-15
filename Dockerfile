# ── Stage 1: Build React webapp ───────────────────────────────────────────────
FROM node:20-slim AS webapp-builder
WORKDIR /app
COPY ai-dj-webapp/package*.json ./
RUN npm ci
COPY ai-dj-webapp/ ./
RUN npm run build

# ── Stage 2: Build ROS2 C++ nodes (Kilted / Ubuntu 24.04 Noble) ───────────────
FROM ros:kilted-ros-base-noble AS ros-builder
RUN apt-get update && apt-get install -y --no-install-recommends \
    curl \
    libsndfile1-dev \
    python3-pip \
    python3-colcon-common-extensions \
    && rm -rf /var/lib/apt/lists/*

# Install ONNX Runtime prebuilt binary (arm64 → aarch64, x86_64 → x64)
ARG ONNX_VERSION=1.20.1
RUN ARCH=$(dpkg --print-architecture) && \
    if [ "$ARCH" = "arm64" ]; then ONNX_ARCH="aarch64"; else ONNX_ARCH="x64"; fi && \
    curl -fsSL \
      "https://github.com/microsoft/onnxruntime/releases/download/v${ONNX_VERSION}/onnxruntime-linux-${ONNX_ARCH}-${ONNX_VERSION}.tgz" \
      | tar -xz -C /opt && \
    mv /opt/onnxruntime-linux-${ONNX_ARCH}-${ONNX_VERSION} /opt/onnxruntime

WORKDIR /ros2_ws
COPY src/ src/
RUN . /opt/ros/kilted/setup.sh && \
    CMAKE_PREFIX_PATH="/opt/onnxruntime:${CMAKE_PREFIX_PATH:-}" \
    colcon build || { echo "colcon build FAILED"; exit 1; }

# ── Stage 3: Runtime image ────────────────────────────────────────────────────
FROM ros:kilted-ros-base-noble AS runtime
RUN apt-get update && apt-get install -y --no-install-recommends \
    libsndfile1 \
    python3-pip \
    python3-venv \
    && rm -rf /var/lib/apt/lists/*

# Use a venv so pip can upgrade packages like typing-extensions without
# conflicting with the Debian-managed versions (which have no RECORD file).
# --system-site-packages keeps ROS2's rclpy and friends visible inside the venv.
COPY requirements.txt /tmp/requirements.txt
RUN python3 -m venv /opt/venv --system-site-packages && \
    /opt/venv/bin/pip install --no-cache-dir -r /tmp/requirements.txt

ENV PATH="/opt/venv/bin:$PATH"

COPY --from=ros-builder /opt/onnxruntime /opt/onnxruntime
COPY --from=ros-builder /ros2_ws/install /ros2_ws/install
COPY --from=webapp-builder /app/dist /ros2_ws/ai-dj-webapp/dist

# models/ and sounds/ are volume-mounted at runtime (see docker-compose.yml)
RUN mkdir -p /ros2_ws/models /ros2_ws/sounds

ENV AI_DJ_WORKSPACE=/ros2_ws
WORKDIR /ros2_ws
EXPOSE 8000

CMD ["/bin/bash", "-c", \
  "source /opt/ros/kilted/setup.bash && \
   source /ros2_ws/install/setup.bash && \
   LD_LIBRARY_PATH=/opt/onnxruntime/lib:${LD_LIBRARY_PATH:-} \
   ros2 launch cpp_pkg bringup.launch.py"]
