from launch import LaunchDescription
from launch_ros.actions import Node
import os

def generate_launch_description():
    """
    Launch three fine-tuned YAMNet classification nodes.
    Each node:
    - Loads a different fine-tuned model with unique class labels
    - Publishes to a unique topic
    - Has a unique RGB color for LED visualization
    """
    
    # Get workspace path
    workspace = os.path.expanduser('~/iaac/ai4all/rosnetwork')
    models_path = os.path.join(workspace, 'models')
    
    # Model 1: Red (255, 0, 0)
    yamnet_model1 = Node(
        package='cpp_pkg',
        executable='yamnet_classification',
        name='yamnet_model1',
        output='screen',
        parameters=[{
            'model_path': os.path.join(models_path, 'model1.onnx'),
            'class_names_path': os.path.join(models_path, 'model1_classes.txt'),
            'model_name': 'Model_1_Red',
            'output_topic': 'classification_results_model1',
            'model_color_r': 255,
            'model_color_g': 0,
            'model_color_b': 0,
            'input_sample_rate': 44100,
            'yamnet_sample_rate': 16000
        }]
    )
    
    # Model 2: Green (0, 255, 0)
    yamnet_model2 = Node(
        package='cpp_pkg',
        executable='yamnet_classification',
        name='yamnet_model2',
        output='screen',
        parameters=[{
            'model_path': os.path.join(models_path, 'model2.onnx'),
            'class_names_path': os.path.join(models_path, 'model2_classes.txt'),
            'model_name': 'Model_2_Green',
            'output_topic': 'classification_results_model2',
            'model_color_r': 0,
            'model_color_g': 255,
            'model_color_b': 0,
            'input_sample_rate': 44100,
            'yamnet_sample_rate': 16000
        }]
    )
    
    # Model 3: Blue (0, 0, 255)
    yamnet_model3 = Node(
        package='cpp_pkg',
        executable='yamnet_classification',
        name='yamnet_model3',
        output='screen',
        parameters=[{
            'model_path': os.path.join(models_path, 'model3.onnx'),
            'class_names_path': os.path.join(models_path, 'model3_classes.txt'),
            'model_name': 'Model_3_Blue',
            'output_topic': 'classification_results_model3',
            'model_color_r': 0,
            'model_color_g': 0,
            'model_color_b': 255,
            'input_sample_rate': 44100,
            'yamnet_sample_rate': 16000
        }]
    )
    
    return LaunchDescription([
        yamnet_model1,
        yamnet_model2,
        yamnet_model3
    ])
