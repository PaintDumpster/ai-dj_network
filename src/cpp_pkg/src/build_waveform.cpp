#include <rclcpp/rclcpp.hpp>
#include <std_msgs/msg/string.hpp>
#include <vector>
#include <string>
#include <chrono>
#include <fstream>
#include <filesystem>

class WaveformBuilder : public rclcpp::Node
{
public:
    WaveformBuilder() : Node("waveform_builder"), recording_active_(false)
    {
        // Declare parameters
        this->declare_parameter("sounds_folder", std::string(std::getenv("HOME")) + "/rosnetwork/sounds");
        this->declare_parameter("recording_duration", 30.0);
        this->declare_parameter("sample_rate", 44100);
        
        sounds_folder_ = this->get_parameter("sounds_folder").as_string();
        recording_duration_ = this->get_parameter("recording_duration").as_double();
        sample_rate_ = this->get_parameter("sample_rate").as_int();
        
        // Initialize waveform matrix (for now, storing time and amplitude values)
        waveform_matrix_.clear();
        
        // Subscribe to Arduino data
        subscription_ = this->create_subscription<std_msgs::msg::String>(
            "arduino_data", 10,
            std::bind(&WaveformBuilder::arduino_callback, this, std::placeholders::_1)
        );
        
        // Load sound file paths
        load_sound_mappings();
        
        RCLCPP_INFO(this->get_logger(), "Waveform Builder initialized");
        RCLCPP_INFO(this->get_logger(), "Sounds folder: %s", sounds_folder_.c_str());
        RCLCPP_INFO(this->get_logger(), "Recording duration: %.1f seconds", recording_duration_);
        RCLCPP_INFO(this->get_logger(), "Waiting for button presses to start recording...");
    }

private:
    void load_sound_mappings()
    {
        // Map button numbers (1-10) to sound files
        for (int i = 1; i <= 10; i++) {
            std::string sound_file = sounds_folder_ + "/button_" + std::to_string(i) + ".wav";
            sound_mappings_[i] = sound_file;
        }
        
        // Check which sound files exist
        RCLCPP_INFO(this->get_logger(), "Sound mappings:");
        for (const auto& pair : sound_mappings_) {
            bool exists = std::filesystem::exists(pair.second);
            RCLCPP_INFO(this->get_logger(), "  Button %d -> %s [%s]", 
                       pair.first, pair.second.c_str(), 
                       exists ? "EXISTS" : "MISSING");
        }
    }
    
    void arduino_callback(const std_msgs::msg::String::SharedPtr msg)
    {
        // Start recording on first button press
        if (!recording_active_) {
            start_recording();
        }
        
        // Check if recording time has elapsed
        auto current_time = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            current_time - recording_start_time_).count();
        
        if (elapsed >= recording_duration_) {
            if (recording_active_) {
                stop_recording();
            }
            return;
        }
        
        // Parse button number from Arduino data
        try {
            int button_number = std::stoi(msg->data);
            
            if (button_number >= 1 && button_number <= 10) {
                process_button_press(button_number, elapsed);
            } else {
                RCLCPP_WARN(this->get_logger(), "Invalid button number: %d", button_number);
            }
        } catch (const std::exception& e) {
            RCLCPP_WARN(this->get_logger(), "Failed to parse button data: %s", msg->data.c_str());
        }
    }
    
    void start_recording()
    {
        recording_active_ = true;
        recording_start_time_ = std::chrono::steady_clock::now();
        waveform_matrix_.clear();
        
        RCLCPP_INFO(this->get_logger(), "=== RECORDING STARTED ===");
        RCLCPP_INFO(this->get_logger(), "You have %.1f seconds to press buttons!", recording_duration_);
    }
    
    void stop_recording()
    {
        recording_active_ = false;
        
        RCLCPP_INFO(this->get_logger(), "=== RECORDING STOPPED ===");
        RCLCPP_INFO(this->get_logger(), "Total waveform points: %zu", waveform_matrix_.size());
        
        // Save waveform to file
        save_waveform();
        
        RCLCPP_INFO(this->get_logger(), "Ready to start new recording on next button press");
    }
    
    void process_button_press(int button_number, double time_elapsed)
    {
        RCLCPP_INFO(this->get_logger(), "Button %d pressed at %.2f seconds", 
                   button_number, time_elapsed);
        
        // Get the sound file for this button
        std::string sound_file = sound_mappings_[button_number];
        
        // Check if sound file exists
        if (!std::filesystem::exists(sound_file)) {
            RCLCPP_WARN(this->get_logger(), "Sound file not found: %s", sound_file.c_str());
            // For now, generate a placeholder waveform
            generate_placeholder_waveform(button_number, time_elapsed);
        } else {
            // Load and add sound waveform to the matrix
            load_and_add_sound(sound_file, button_number, time_elapsed);
        }
        
        RCLCPP_INFO(this->get_logger(), "Current waveform size: %zu points", waveform_matrix_.size());
    }
    
    void generate_placeholder_waveform(int button_number, double time_offset)
    {
        // Generate a simple sine wave as placeholder
        // Frequency based on button number (e.g., button 1 = 220Hz, button 10 = 880Hz)
        double frequency = 220.0 + (button_number - 1) * 73.33;  // A3 to A5 range
        double duration = 0.5;  // 0.5 second sound
        int num_samples = static_cast<int>(duration * sample_rate_);
        
        for (int i = 0; i < num_samples; i++) {
            double t = time_offset + (static_cast<double>(i) / sample_rate_);
            double amplitude = 0.5 * std::sin(2.0 * M_PI * frequency * (static_cast<double>(i) / sample_rate_));
            
            // Apply envelope (fade in/out)
            if (i < sample_rate_ * 0.05) {
                amplitude *= static_cast<double>(i) / (sample_rate_ * 0.05);
            } else if (i > num_samples - sample_rate_ * 0.05) {
                amplitude *= static_cast<double>(num_samples - i) / (sample_rate_ * 0.05);
            }
            
            waveform_matrix_.push_back({t, amplitude});
        }
        
        RCLCPP_INFO(this->get_logger(), "Generated placeholder waveform: %.0fHz, %.1fs", 
                   frequency, duration);
    }
    
    void load_and_add_sound(const std::string& sound_file, int button_number, double time_offset)
    {
        // TODO: Implement actual WAV file loading
        // For now, use placeholder
        RCLCPP_INFO(this->get_logger(), "Loading sound file: %s", sound_file.c_str());
        generate_placeholder_waveform(button_number, time_offset);
    }
    
    void save_waveform()
    {
        std::string output_file = "/tmp/waveform_" + 
            std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) + ".csv";
        
        std::ofstream file(output_file);
        if (file.is_open()) {
            file << "time,amplitude\n";
            for (const auto& point : waveform_matrix_) {
                file << point.time << "," << point.amplitude << "\n";
            }
            file.close();
            RCLCPP_INFO(this->get_logger(), "Waveform saved to: %s", output_file.c_str());
        } else {
            RCLCPP_ERROR(this->get_logger(), "Failed to save waveform to file");
        }
    }
    
    // Waveform data structure
    struct WaveformPoint {
        double time;      // Time in seconds
        double amplitude; // Amplitude value (-1.0 to 1.0)
    };
    
    // Member variables
    rclcpp::Subscription<std_msgs::msg::String>::SharedPtr subscription_;
    
    std::string sounds_folder_;
    double recording_duration_;
    int sample_rate_;
    
    std::map<int, std::string> sound_mappings_;
    std::vector<WaveformPoint> waveform_matrix_;
    
    bool recording_active_;
    std::chrono::steady_clock::time_point recording_start_time_;
};

int main(int argc, char** argv)
{
    rclcpp::init(argc, argv);
    auto node = std::make_shared<WaveformBuilder>();
    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;
}
