"""
AI DJ — Full System Bringup
============================
Startup order
  t = 0 s  : reader, build_waveform, writer
  t = 0 s  : yamnet_surveillance, yamnet_natural, yamnet_cultural  (ONNX loading is slow)
  t = 8 s  : web_bridge  (waits for service servers to be ready)
  t = 10 s : llm_node    (optional, only if ANTHROPIC_API_KEY is set)

Arguments
  with_llm   (default true)  — set to false to skip the LLM node
  with_writer(default false) — set to true when Pico serial writers are connected
  ws_delay   (default 8.0)   — seconds to wait before starting web_bridge;
                                increase if yamnet ONNX loading is slow on your hardware
"""

import os
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, TimerAction, LogInfo
from launch.conditions import IfCondition
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description():

    workspace = os.environ.get(
        'AI_DJ_WORKSPACE',
        os.path.expanduser('~/iaac/ai4all/rosnetwork')  # overridden to /ros2_ws inside Docker
    )
    models_path = os.path.join(workspace, 'models')
    sounds_path = os.path.join(workspace, 'sounds')

    # ── Launch arguments ──────────────────────────────────────────────────────
    arg_with_llm = DeclareLaunchArgument(
        'with_llm',
        default_value='true',
        description='Start the LLM node (requires ANTHROPIC_API_KEY env var)',
    )
    arg_with_writer = DeclareLaunchArgument(
        'with_writer',
        default_value='false',
        description='Start the LED matrix writer node (requires Pico serial connections)',
    )
    arg_ws_delay = DeclareLaunchArgument(
        'ws_delay',
        default_value='8.0',
        description='Seconds to wait before starting web_bridge (give ONNX models time to load)',
    )
    arg_llm_delay = DeclareLaunchArgument(
        'llm_delay',
        default_value='12.0',
        description='Seconds to wait before starting llm_node (must be > ws_delay)',
    )

    with_llm    = LaunchConfiguration('with_llm')
    with_writer = LaunchConfiguration('with_writer')
    ws_delay    = LaunchConfiguration('ws_delay')
    llm_delay   = LaunchConfiguration('llm_delay')

    # ── Group 1 (t=0) — Hardware I/O & waveform builder ─────────────────────
    node_reader = Node(
        package='cpp_pkg',
        executable='reader',
        name='reader',
        output='screen',
        emulate_tty=True,
        parameters=[{
            'port': '/dev/ttyACM0',
            'baud_rate': 9600,
        }],
    )

    node_build_waveform = Node(
        package='cpp_pkg',
        executable='build_waveform',
        name='waveform_builder',
        output='screen',
        emulate_tty=True,
        parameters=[{
            'sounds_folder':       sounds_path,
            'recording_duration':  30.0,
            'sample_rate':         44100,
            'matrix_update_rate':  0.1,
        }],
    )

    node_writer = Node(
        package='cpp_pkg',
        executable='writer',
        name='led_writer',
        output='screen',
        emulate_tty=True,
        condition=IfCondition(with_writer),
    )

    # ── Group 2 (t=0) — YAMNet classifiers (ONNX loading takes several seconds) ─
    node_yamnet_surveillance = Node(
        package='cpp_pkg',
        executable='yamnet_classification',
        name='yamnet_surveillance',
        output='screen',
        emulate_tty=True,
        parameters=[{
            'model_path':        os.path.join(models_path, 'surveillance_head.onnx'),
            'class_names_path':  os.path.join(models_path, 'surveillance_classes.txt'),
            'model_name':        'surveillance',
            'output_topic':      'classification_results_surveillance',
            'input_sample_rate': 44100,
            'yamnet_sample_rate': 16000,
            'model_color_r':     255,
            'model_color_g':     0,
            'model_color_b':     0,
        }],
    )

    node_yamnet_natural = Node(
        package='cpp_pkg',
        executable='yamnet_classification',
        name='yamnet_natural',
        output='screen',
        emulate_tty=True,
        parameters=[{
            'model_path':        os.path.join(models_path, 'natural_head.onnx'),
            'class_names_path':  os.path.join(models_path, 'natural_classes.txt'),
            'model_name':        'natural',
            'output_topic':      'classification_results_natural',
            'input_sample_rate': 44100,
            'yamnet_sample_rate': 16000,
            'model_color_r':     0,
            'model_color_g':     255,
            'model_color_b':     0,
        }],
    )

    node_yamnet_cultural = Node(
        package='cpp_pkg',
        executable='yamnet_classification',
        name='yamnet_cultural',
        output='screen',
        emulate_tty=True,
        parameters=[{
            'model_path':        os.path.join(models_path, 'cultural_head.onnx'),
            'class_names_path':  os.path.join(models_path, 'cultural_classes.txt'),
            'model_name':        'cultural',
            'output_topic':      'classification_results_cultural',
            'input_sample_rate': 44100,
            'yamnet_sample_rate': 16000,
            'model_color_r':     0,
            'model_color_g':     0,
            'model_color_b':     255,
        }],
    )

    # ── Group 3 (t = ws_delay) — Web bridge ──────────────────────────────────
    # Delayed so all C++ service servers (build_waveform + yamnet nodes) are ready.
    node_web_bridge = Node(
        package='py_pkg',
        executable='web_bridge',
        name='web_bridge',
        output='screen',
        emulate_tty=True,
    )

    # ── Group 4 (t = ws_delay + 2 s) — LLM node (optional) ──────────────────
    node_llm = Node(
        package='py_pkg',
        executable='llm_node',
        name='llm_node',
        output='screen',
        emulate_tty=True,
        condition=IfCondition(with_llm),
        # ANTHROPIC_API_KEY must be set in the environment before launching
    )

    return LaunchDescription([
        # Arguments
        arg_with_llm,
        arg_with_writer,
        arg_ws_delay,
        arg_llm_delay,

        # t = 0 — hardware I/O + waveform builder
        LogInfo(msg='[bringup] Starting reader, waveform builder, writer…'),
        node_reader,
        node_build_waveform,
        node_writer,

        # t = 0 — YAMNet classifiers (load ONNX models in parallel)
        LogInfo(msg='[bringup] Starting YAMNet classifiers (surveillance / natural / cultural)…'),
        node_yamnet_surveillance,
        node_yamnet_natural,
        node_yamnet_cultural,

        # t = ws_delay — web bridge (waits for service servers to be ready)
        TimerAction(
            period=ws_delay,
            actions=[
                LogInfo(msg='[bringup] Starting web_bridge (FastAPI + WebSocket)…'),
                node_web_bridge,
            ],
        ),

        # t = llm_delay — LLM node (optional, default 12 s)
        TimerAction(
            period=llm_delay,
            actions=[
                LogInfo(msg='[bringup] Starting llm_node (optional)…'),
                node_llm,
            ],
        ),
    ])
