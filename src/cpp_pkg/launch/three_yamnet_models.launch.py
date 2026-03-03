from launch import LaunchDescription
from launch_ros.actions import Node
import os

def generate_launch_description():
    """
    Launch three YAMNet classification nodes with different fine-tuned models.
    Each node:
    - Loads a different model
    - Publishes to a unique topic
    - Has a unique name identifier
    """
    
    # Get workspace path from environment or use home directory
    workspace = os.path.expanduser('~/rosnetwork')
    models_path = os.path.join(workspace, 'models')
    
    # Model 1: Original YAMNet
    yamnet_model1 = Node(
        package='cpp_pkg',
        executable='yamnet_classification',
        name='yamnet_model1',
        output='screen',
        parameters=[{
            'model_path': os.path.join(models_path, 'yamnet_model1.onnx'),
            'class_names_path': os.path.join(models_path, 'yamnet_classes.txt'),
            'model_name': 'Model_1_Original',
            'output_topic': 'classification_results_model1',
            'input_sample_rate': 44100,
            'yamnet_sample_rate': 16000
        }]
    )
    
    # Model 2: Fine-tuned on Dataset A
    yamnet_model2 = Node(
        package='cpp_pkg',
        executable='yamnet_classification',
        name='yamnet_model2',
        output='screen',
        parameters=[{
            'model_path': os.path.join(models_path, 'yamnet_model2.onnx'),
            'class_names_path': os.path.join(models_path, 'yamnet_classes.txt'),
            'model_name': 'Model_2_DatasetA',
            'output_topic': 'classification_results_model2',
            'input_sample_rate': 44100,
            'yamnet_sample_rate': 16000
        }]
    )
    
    # Model 3: Fine-tuned on Dataset B
    yamnet_model3 = Node(
        package='cpp_pkg',
        executable='yamnet_classification',
        name='yamnet_model3',
        output='screen',
        parameters=[{
            'model_path': os.path.join(models_path, 'yamnet_model3.onnx'),
            'class_names_path': os.path.join(models_path, 'yamnet_classes.txt'),
            'model_name': 'Model_3_DatasetB',
            'output_topic': 'classification_results_model3',
            'input_sample_rate': 44100,
            'yamnet_sample_rate': 16000
        }]
    )
    
    return LaunchDescription([
        yamnet_model1,
        yamnet_model2,
        yamnet_model3
    ])
