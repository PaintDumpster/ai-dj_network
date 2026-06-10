from launch import LaunchDescription
from launch_ros.actions import Node
import os

def generate_launch_description():
    """
    Launch three fine-tuned YAMNet classification nodes.
    Each node:
    - Uses YamNet.onnx for embedding extraction (Stage 1)
    - Loads a different ONNX classification head with unique class labels (Stage 2)
    - Publishes classification results
    """
    
    # Resolve workspace path — set AI_DJ_WORKSPACE in ~/.bashrc to match your clone location.
    # Falls back to the original default if the variable is not set.
    workspace = os.environ.get(
        'AI_DJ_WORKSPACE',
        os.path.expanduser('~/iaac/ai4all/rosnetwork')
    )
    models_path = os.path.join(workspace, 'models')
    
    # Surveillance Model: Red (255, 0, 0)
    yamnet_surveillance = Node(
        package='cpp_pkg',
        executable='yamnet_classification',
        name='yamnet_surveillance',
        output='screen',
        parameters=[{
            'model_path': os.path.join(models_path, 'surveillance_head.onnx'),
            'class_names_path': os.path.join(models_path, 'surveillance_classes.txt'),
            'model_name': 'surveillance',
            'output_topic': 'classification_results_surveillance',
            'input_sample_rate': 44100,
            'yamnet_sample_rate': 16000,
            'model_color_r': 255,
            'model_color_g': 0,
            'model_color_b': 0
        }]
    )
    
    # Natural Model: Green (0, 255, 0)
    yamnet_natural = Node(
        package='cpp_pkg',
        executable='yamnet_classification',
        name='yamnet_natural',
        output='screen',
        parameters=[{
            'model_path': os.path.join(models_path, 'natural_head.onnx'),
            'class_names_path': os.path.join(models_path, 'natural_classes.txt'),
            'model_name': 'natural',
            'output_topic': 'classification_results_natural',
            'input_sample_rate': 44100,
            'yamnet_sample_rate': 16000,
            'model_color_r': 0,
            'model_color_g': 255,
            'model_color_b': 0
        }]
    )
    
    # Cultural Model: Blue (0, 0, 255)
    yamnet_cultural = Node(
        package='cpp_pkg',
        executable='yamnet_classification',
        name='yamnet_cultural',
        output='screen',
        parameters=[{
            'model_path': os.path.join(models_path, 'cultural_head.onnx'),
            'class_names_path': os.path.join(models_path, 'cultural_classes.txt'),
            'model_name': 'cultural',
            'output_topic': 'classification_results_cultural',
            'input_sample_rate': 44100,
            'yamnet_sample_rate': 16000,
            'model_color_r': 0,
            'model_color_g': 0,
            'model_color_b': 255
        }]
    )
    
    return LaunchDescription([
        yamnet_surveillance,
        yamnet_natural,
        yamnet_cultural
    ])
