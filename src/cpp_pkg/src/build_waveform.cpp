#include <rclcpp/rclcpp.hpp>
#include <std_msgs/msg/string.hpp>
#include <std_msgs/msg/u_int8_multi_array.hpp>
#include <std_msgs/msg/float32_multi_array.hpp>
#include <std_srvs/srv/trigger.hpp>
#include <vector>
#include <string>
#include <chrono>
#include <fstream>
#include <filesystem>
#include <cmath>
#include <random>
#include <sndfile.h>

class WaveformBuilder : public rclcpp::Node
{
public:
    WaveformBuilder() : Node("waveform_builder"), recording_active_(false)
    {
        // LED Matrix dimensions
        matrix_height_ = 74;   // Amplitude bins
        matrix_width_ = 75;    // Time bins (30 seconds)
        
        // Initialize LED matrix
        led_matrix_.resize(matrix_height_ * matrix_width_, 0);
        
        // Declare parameters
        this->declare_parameter("sounds_folder", std::string(std::getenv("HOME")) + "/iaac/ai4all/rosnetwork/sounds");
        this->declare_parameter("recording_duration", 30.0);
        this->declare_parameter("sample_rate", 44100);
        this->declare_parameter("matrix_update_rate", 0.1);  // Update LEDs every 100ms
        
        sounds_folder_ = this->get_parameter("sounds_folder").as_string();
        recording_duration_ = this->get_parameter("recording_duration").as_double();
        sample_rate_ = this->get_parameter("sample_rate").as_int();
        double matrix_update_rate = this->get_parameter("matrix_update_rate").as_double();
        
        // Initialize waveform matrix (for now, storing time and amplitude values)
        waveform_matrix_.clear();
        
        // Subscribe to Arduino data
        subscription_ = this->create_subscription<std_msgs::msg::String>(
            "arduino_data", 10,
            std::bind(&WaveformBuilder::arduino_callback, this, std::placeholders::_1)
        );
        
        // NOTE: Button 11 (state_control) is now handled by web_bridge node
        // build_waveform only responds to service calls (start_recording/stop_recording)
        // state_control_subscription_ commented out to avoid conflicts with webapp state machine
        
        // Publisher for LED matrix data (74×75 full matrix, grayscale uint8)
        matrix_publisher_ = this->create_publisher<std_msgs::msg::UInt8MultiArray>("led_matrix", 10);

        // Pico 1 (clusters 1-3, cols 0-44): normalized waveform submatrix (74×45)
        // Pico 2 (clusters 4-5, cols 45-74): normalized waveform submatrix (74×30)
        // Both published once when stop_recording is called.
        pico_waveform_pub_1_ = this->create_publisher<std_msgs::msg::Float32MultiArray>("pico_waveform_1", 10);
        pico_waveform_pub_2_ = this->create_publisher<std_msgs::msg::Float32MultiArray>("pico_waveform_2", 10);
        
        // Timer to periodically publish matrix updates
        matrix_timer_ = this->create_wall_timer(
            std::chrono::duration<double>(matrix_update_rate),
            std::bind(&WaveformBuilder::publish_matrix, this)
        );
        
        // Services for external control
        start_recording_service_ = this->create_service<std_srvs::srv::Trigger>(
            "start_recording",
            std::bind(&WaveformBuilder::handle_start_recording, this, std::placeholders::_1, std::placeholders::_2)
        );
        
        stop_recording_service_ = this->create_service<std_srvs::srv::Trigger>(
            "stop_recording",
            std::bind(&WaveformBuilder::handle_stop_recording, this, std::placeholders::_1, std::placeholders::_2)
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
        // Scan for district folders (1-10) and load all .wav files
        RCLCPP_INFO(this->get_logger(), "Loading sound mappings from: %s", sounds_folder_.c_str());
        
        for (int button = 1; button <= 10; button++) {
            std::vector<std::string> wav_files;
            
            // Try to find a folder that starts with the button number
            for (const auto& entry : std::filesystem::directory_iterator(sounds_folder_)) {
                if (!entry.is_directory()) continue;
                
                std::string folder_name = entry.path().filename().string();
                // Check if folder starts with button number (e.g., "1. Ciutat Vella")
                if (folder_name.find(std::to_string(button) + ".") == 0) {
                    // Scan for .wav files in this folder
                    for (const auto& file : std::filesystem::directory_iterator(entry.path())) {
                        if (file.path().extension() == ".wav") {
                            wav_files.push_back(file.path().string());
                        }
                    }
                    break;
                }
            }
            
            if (!wav_files.empty()) {
                sound_mappings_[button] = wav_files;
                RCLCPP_INFO(this->get_logger(), "  Button %d: %zu sound(s) found", 
                           button, wav_files.size());
            } else {
                RCLCPP_WARN(this->get_logger(), "  Button %d: No .wav files found (will use placeholder)", 
                           button);
            }
        }
        
        // Initialize random number generator
        random_generator_.seed(std::chrono::system_clock::now().time_since_epoch().count());
    }
    
    void state_control_callback(const std_msgs::msg::String::SharedPtr msg)
    {
        // DISABLED: web_bridge now handles button 11 state control
        // This node only responds to service calls
        (void)msg;
        return;
    }
    
    void arduino_callback(const std_msgs::msg::String::SharedPtr msg)
    {
        // Only process if recording is active (controlled by service call or button 11)
        if (!recording_active_) {
            return;
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
        
        // Parse new protocol: only PRESS_n events trigger sound (RELEASE_n is ignored here)
        const std::string& token = msg->data;
        const std::string press_prefix = "PRESS_";
        if (token.rfind(press_prefix, 0) != 0) {
            return; // RELEASE_n or unexpected token — ignore
        }
        try {
            int button_number = std::stoi(token.substr(press_prefix.size()));
            if (button_number >= 1 && button_number <= 10) {
                process_button_press(button_number, elapsed);
            } else {
                RCLCPP_WARN(this->get_logger(), "Invalid button number: %d", button_number);
            }
        } catch (const std::exception& e) {
            RCLCPP_WARN(this->get_logger(), "Failed to parse button data: %s", token.c_str());
        }
    }
    
    void handle_start_recording(
        const std::shared_ptr<std_srvs::srv::Trigger::Request> request,
        std::shared_ptr<std_srvs::srv::Trigger::Response> response)
    {
        (void)request;
        
        if (recording_active_) {
            response->success = false;
            response->message = "Recording already in progress";
            return;
        }
        
        start_recording();
        response->success = true;
        response->message = "Recording started";
        RCLCPP_INFO(this->get_logger(), "Recording started via service call");
    }
    
    void handle_stop_recording(
        const std::shared_ptr<std_srvs::srv::Trigger::Request> request,
        std::shared_ptr<std_srvs::srv::Trigger::Response> response)
    {
        (void)request;
        
        if (!recording_active_) {
            response->success = false;
            response->message = "No recording in progress";
            return;
        }
        
        stop_recording();
        response->success = true;
        response->message = "Recording stopped. Waveform saved to: " + last_waveform_file_;
        RCLCPP_INFO(this->get_logger(), "Recording stopped via service call");
    }
    
    void start_recording()
    {
        recording_active_ = true;
        recording_start_time_ = std::chrono::steady_clock::now();
        waveform_matrix_.clear();
        
        // Clear LED matrix
        std::fill(led_matrix_.begin(), led_matrix_.end(), 0);
        
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

        // Publish normalized waveform for Pico boards
        publish_pico_waveform();

        RCLCPP_INFO(this->get_logger(), "Ready to start new recording on next button press");
    }
    
    void process_button_press(int button_number, double time_elapsed)
    {
        RCLCPP_INFO(this->get_logger(), "Button %d pressed at %.2f seconds", 
                   button_number, time_elapsed);
        
        // Check if this button has any sound files
        if (sound_mappings_.find(button_number) == sound_mappings_.end() || 
            sound_mappings_[button_number].empty()) {
            RCLCPP_WARN(this->get_logger(), "No sounds available for button %d, using placeholder", 
                       button_number);
            generate_placeholder_waveform(button_number, time_elapsed);
        } else {
            // Randomly select a sound file from the available options
            const auto& sound_files = sound_mappings_[button_number];
            std::uniform_int_distribution<size_t> distribution(0, sound_files.size() - 1);
            size_t random_index = distribution(random_generator_);
            std::string selected_file = sound_files[random_index];
            
            std::filesystem::path file_path(selected_file);
            RCLCPP_INFO(this->get_logger(), "Selected: %s", file_path.filename().string().c_str());
            
            // Load and add sound waveform to the matrix
            load_and_add_sound(selected_file, button_number, time_elapsed);
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
        
        // Update LED matrix
        update_led_matrix();
        
        RCLCPP_INFO(this->get_logger(), "Generated placeholder waveform: %.0fHz, %.1fs", 
                   frequency, duration);
    }
    
    void load_and_add_sound(const std::string& sound_file, int button_number, double time_offset)
    {
        // Open WAV file using libsndfile
        SF_INFO sf_info;
        SNDFILE* sf = sf_open(sound_file.c_str(), SFM_READ, &sf_info);
        
        if (!sf) {
            RCLCPP_ERROR(this->get_logger(), "Failed to open sound file: %s", sound_file.c_str());
            RCLCPP_ERROR(this->get_logger(), "libsndfile error: %s", sf_strerror(nullptr));
            generate_placeholder_waveform(button_number, time_offset);
            return;
        }
        
        // Read audio data
        std::vector<float> audio_buffer(sf_info.frames * sf_info.channels);
        sf_count_t frames_read = sf_readf_float(sf, audio_buffer.data(), sf_info.frames);
        sf_close(sf);
        
        if (frames_read <= 0) {
            RCLCPP_ERROR(this->get_logger(), "Failed to read audio data from: %s", sound_file.c_str());
            generate_placeholder_waveform(button_number, time_offset);
            return;
        }
        
        RCLCPP_INFO(this->get_logger(), "Loaded WAV: %ld frames, %d channels, %d Hz", 
                   frames_read, sf_info.channels, sf_info.samplerate);
        
        // Convert to mono if stereo (average channels)
        std::vector<float> mono_audio;
        if (sf_info.channels == 1) {
            mono_audio = audio_buffer;
        } else {
            mono_audio.resize(frames_read);
            for (sf_count_t i = 0; i < frames_read; i++) {
                float sum = 0.0f;
                for (int ch = 0; ch < sf_info.channels; ch++) {
                    sum += audio_buffer[i * sf_info.channels + ch];
                }
                mono_audio[i] = sum / sf_info.channels;
            }
        }
        
        // Resample if needed (simple linear interpolation)
        // YAMNet expects 16kHz, but we work with our sample_rate_ parameter
        double time_per_sample = 1.0 / sf_info.samplerate;
        
        // Add audio data to waveform matrix
        for (size_t i = 0; i < mono_audio.size(); i++) {
            double t = time_offset + (i * time_per_sample);
            double amplitude = static_cast<double>(mono_audio[i]);
            
            // Clamp amplitude to [-1, 1]
            amplitude = std::max(-1.0, std::min(1.0, amplitude));
            
            waveform_matrix_.push_back({t, amplitude});
        }
        
        // Update LED matrix
        update_led_matrix();
        
        RCLCPP_INFO(this->get_logger(), "Added %zu samples to waveform", mono_audio.size());
    }
    
    void update_led_matrix()
    {
        // Convert waveform_matrix_ to LED matrix representation
        // LED matrix is 42 (height/amplitude) x 146 (width/time)
        
        // Clear previous matrix
        std::fill(led_matrix_.begin(), led_matrix_.end(), 0);
        
        if (waveform_matrix_.empty()) {
            return;
        }
        
        double time_per_column = recording_duration_ / matrix_width_;
        
        // For each waveform point, map it to the LED matrix
        for (const auto& point : waveform_matrix_) {
            // Map time to column (0 to matrix_width_-1)
            int col = static_cast<int>(point.time / time_per_column);
            if (col < 0 || col >= matrix_width_) {
                continue;
            }
            
            // Map amplitude (-1.0 to 1.0) to row (0 to matrix_height_-1)
            // Center is at matrix_height_/2
            int row = static_cast<int>((matrix_height_ / 2.0) - (point.amplitude * matrix_height_ / 2.0));
            row = std::max(0, std::min(matrix_height_ - 1, row));
            
            // Set the pixel (store intensity 0-255, for now just use 255 for white)
            int index = row * matrix_width_ + col;
            led_matrix_[index] = 255;  // White color for base waveform
        }
    }
    
    void publish_matrix()
    {
        if (!recording_active_) {
            return;
        }
        
        auto msg = std_msgs::msg::UInt8MultiArray();
        
        // Set dimensions: height x width
        msg.layout.dim.resize(2);
        msg.layout.dim[0].label = "height";
        msg.layout.dim[0].size = matrix_height_;
        msg.layout.dim[0].stride = matrix_height_ * matrix_width_;
        msg.layout.dim[1].label = "width";
        msg.layout.dim[1].size = matrix_width_;
        msg.layout.dim[1].stride = matrix_width_;
        
        // Copy matrix data
        msg.data = led_matrix_;
        
        matrix_publisher_->publish(msg);
    }
    
    void publish_pico_waveform()
    {
        // Split the 74×75 matrix column-wise between two Pico boards:
        //   Pico 1 — clusters 1-3 → columns 0..44  (74×45, covers ~18 s)
        //   Pico 2 — clusters 4-5 → columns 45..74 (74×30, covers ~12 s)
        static constexpr int CLUSTER_COLS = 15;
        static constexpr int PICO1_COLS   = 3 * CLUSTER_COLS;  // 45
        static constexpr int PICO2_COLS   = 2 * CLUSTER_COLS;  // 30

        auto make_msg = [&](int col_start, int col_count) {
            auto msg = std_msgs::msg::Float32MultiArray();
            msg.layout.dim.resize(2);
            msg.layout.dim[0].label  = "height";
            msg.layout.dim[0].size   = matrix_height_;
            msg.layout.dim[0].stride = matrix_height_ * col_count;
            msg.layout.dim[1].label  = "width";
            msg.layout.dim[1].size   = col_count;
            msg.layout.dim[1].stride = col_count;
            msg.data.reserve(matrix_height_ * col_count);
            for (int row = 0; row < matrix_height_; row++) {
                for (int col = col_start; col < col_start + col_count; col++) {
                    msg.data.push_back(led_matrix_[row * matrix_width_ + col] / 255.0f);
                }
            }
            return msg;
        };

        auto msg1 = make_msg(0, PICO1_COLS);
        pico_waveform_pub_1_->publish(msg1);
        RCLCPP_INFO(this->get_logger(), "Published pico_waveform_1: %zu floats (%dx%d)",
                    msg1.data.size(), matrix_height_, PICO1_COLS);

        auto msg2 = make_msg(PICO1_COLS, PICO2_COLS);
        pico_waveform_pub_2_->publish(msg2);
        RCLCPP_INFO(this->get_logger(), "Published pico_waveform_2: %zu floats (%dx%d)",
                    msg2.data.size(), matrix_height_, PICO2_COLS);
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
            last_waveform_file_ = output_file;
            RCLCPP_INFO(this->get_logger(), "Waveform saved to: %s", output_file.c_str());
        } else {
            last_waveform_file_ = "";
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
    rclcpp::Subscription<std_msgs::msg::String>::SharedPtr state_control_subscription_;
    rclcpp::Publisher<std_msgs::msg::UInt8MultiArray>::SharedPtr matrix_publisher_;
    rclcpp::Publisher<std_msgs::msg::Float32MultiArray>::SharedPtr pico_waveform_pub_1_;
    rclcpp::Publisher<std_msgs::msg::Float32MultiArray>::SharedPtr pico_waveform_pub_2_;
    rclcpp::TimerBase::SharedPtr matrix_timer_;
    
    // Services
    rclcpp::Service<std_srvs::srv::Trigger>::SharedPtr start_recording_service_;
    rclcpp::Service<std_srvs::srv::Trigger>::SharedPtr stop_recording_service_;
    
    std::string sounds_folder_;
    double recording_duration_;
    int sample_rate_;
    
    // LED Matrix dimensions and data
    int matrix_height_;  // 42 rows (amplitude)
    int matrix_width_;   // 146 columns (time)
    std::vector<uint8_t> led_matrix_;  // Flattened matrix data
    
    std::map<int, std::vector<std::string>> sound_mappings_;  // Button -> list of sound files
    std::vector<WaveformPoint> waveform_matrix_;
    std::string last_waveform_file_;
    
    bool recording_active_;
    std::chrono::steady_clock::time_point recording_start_time_;
    
    // Random number generator for sound selection
    std::mt19937 random_generator_;
};

int main(int argc, char** argv)
{
    rclcpp::init(argc, argv);
    auto node = std::make_shared<WaveformBuilder>();
    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;
}
