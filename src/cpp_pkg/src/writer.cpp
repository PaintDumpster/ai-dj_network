#include <iostream>
#include <rclcpp/rclcpp.hpp>
#include <std_msgs/msg/string.hpp>
#include <std_msgs/msg/u_int8_multi_array.hpp>
#include <vector>
#include <string>
#include <sstream>

class LEDMatrixWriter : public rclcpp::Node
{
public:
    LEDMatrixWriter() : Node("led_matrix_writer")
    {
        // LED Matrix dimensions
        matrix_height_ = 42;  // Amplitude bins
        matrix_width_ = 146;  // Time bins (30 seconds)
        
        // Initialize display matrix (RGB for each pixel)
        // Format: R,G,B for each pixel, flattened
        display_matrix_.resize(matrix_height_ * matrix_width_ * 3, 0);
        
        // Declare parameters
        this->declare_parameter("serial_port", std::string("/dev/ttyACM0"));
        this->declare_parameter("update_rate", 0.1);  // 10 Hz update rate
        
        serial_port_ = this->get_parameter("serial_port").as_string();
        double update_rate = this->get_parameter("update_rate").as_double();
        
        // Subscribe to LED matrix data from waveform builder
        matrix_subscription_ = this->create_subscription<std_msgs::msg::UInt8MultiArray>(
            "led_matrix", 10,
            std::bind(&LEDMatrixWriter::matrix_callback, this, std::placeholders::_1)
        );
        
        // Subscribe to paint commands from classifier
        paint_subscription_ = this->create_subscription<std_msgs::msg::String>(
            "led_paint_commands", 10,
            std::bind(&LEDMatrixWriter::paint_callback, this, std::placeholders::_1)
        );
        
        // Timer to send matrix data to Arduino
        send_timer_ = this->create_wall_timer(
            std::chrono::duration<double>(update_rate),
            std::bind(&LEDMatrixWriter::send_to_arduino, this)
        );
        
        RCLCPP_INFO(this->get_logger(), "LED Matrix Writer initialized");
        RCLCPP_INFO(this->get_logger(), "Matrix size: %dx%d", matrix_height_, matrix_width_);
        RCLCPP_INFO(this->get_logger(), "Serial port: %s", serial_port_.c_str());
    }

private:
    void matrix_callback(const std_msgs::msg::UInt8MultiArray::SharedPtr msg)
    {
        if (msg->data.size() != static_cast<size_t>(matrix_height_ * matrix_width_)) {
            RCLCPP_WARN(this->get_logger(), "Received matrix size mismatch: expected %d, got %zu",
                       matrix_height_ * matrix_width_, msg->data.size());
            return;
        }
        
        // Convert grayscale waveform to RGB display matrix
        // White color (255, 255, 255) for base waveform pixels
        for (size_t i = 0; i < msg->data.size(); i++) {
            if (msg->data[i] > 0) {
                // Set RGB channels to white for waveform
                display_matrix_[i * 3] = 255;      // R
                display_matrix_[i * 3 + 1] = 255;  // G
                display_matrix_[i * 3 + 2] = 255;  // B
            } else {
                // Clear if no waveform
                display_matrix_[i * 3] = 0;      // R
                display_matrix_[i * 3 + 1] = 0;  // G
                display_matrix_[i * 3 + 2] = 0;  // B
            }
        }
        
        // Re-apply any paint overlays from paint_regions_
        apply_paint_overlays();
        
        matrix_updated_ = true;
    }
    
    void paint_callback(const std_msgs::msg::String::SharedPtr msg)
    {
        // Parse paint command: "PAINT,start_time,end_time,R,G,B"
        std::string command = msg->data;
        
        std::istringstream ss(command);
        std::string token;
        std::vector<std::string> tokens;
        
        while (std::getline(ss, token, ',')) {
            tokens.push_back(token);
        }
        
        if (tokens.size() != 6 || tokens[0] != "PAINT") {
            RCLCPP_WARN(this->get_logger(), "Invalid paint command: %s", command.c_str());
            return;
        }
        
        try {
            double start_time = std::stod(tokens[1]);
            double end_time = std::stod(tokens[2]);
            uint8_t r = static_cast<uint8_t>(std::stoi(tokens[3]));
            uint8_t g = static_cast<uint8_t>(std::stoi(tokens[4]));
            uint8_t b = static_cast<uint8_t>(std::stoi(tokens[5]));
            
            paint_time_segment(start_time, end_time, r, g, b);
            
            RCLCPP_INFO(this->get_logger(), "Painted segment %.2fs-%.2fs with RGB(%d,%d,%d)",
                       start_time, end_time, r, g, b);
        } catch (const std::exception& e) {
            RCLCPP_ERROR(this->get_logger(), "Failed to parse paint command: %s", e.what());
        }
    }
    
    void paint_time_segment(double start_time, double end_time, uint8_t r, uint8_t g, uint8_t b)
    {
        // Store paint region for re-application
        PaintRegion region;
        region.start_time = start_time;
        region.end_time = end_time;
        region.r = r;
        region.g = g;
        region.b = b;
        paint_regions_.push_back(region);
        
        // Apply paint to display matrix
        const double recording_duration = 30.0;
        double time_per_column = recording_duration / matrix_width_;
        
        int start_col = static_cast<int>(start_time / time_per_column);
        int end_col = static_cast<int>(end_time / time_per_column);
        
        start_col = std::max(0, std::min(matrix_width_ - 1, start_col));
        end_col = std::max(0, std::min(matrix_width_ - 1, end_col));
        
        // Paint all rows in the time segment where waveform exists
        for (int col = start_col; col <= end_col; col++) {
            for (int row = 0; row < matrix_height_; row++) {
                int pixel_idx = row * matrix_width_ + col;
                
                // Only paint where waveform exists (non-black pixels)
                if (display_matrix_[pixel_idx * 3] > 0 ||
                    display_matrix_[pixel_idx * 3 + 1] > 0 ||
                    display_matrix_[pixel_idx * 3 + 2] > 0) {
                    display_matrix_[pixel_idx * 3] = r;
                    display_matrix_[pixel_idx * 3 + 1] = g;
                    display_matrix_[pixel_idx * 3 + 2] = b;
                }
            }
        }
    }
    
    void apply_paint_overlays()
    {
        // Re-apply all stored paint regions
        for (const auto& region : paint_regions_) {
            const double recording_duration = 30.0;
            double time_per_column = recording_duration / matrix_width_;
            
            int start_col = static_cast<int>(region.start_time / time_per_column);
            int end_col = static_cast<int>(region.end_time / time_per_column);
            
            start_col = std::max(0, std::min(matrix_width_ - 1, start_col));
            end_col = std::max(0, std::min(matrix_width_ - 1, end_col));
            
            for (int col = start_col; col <= end_col; col++) {
                for (int row = 0; row < matrix_height_; row++) {
                    int pixel_idx = row * matrix_width_ + col;
                    
                    if (display_matrix_[pixel_idx * 3] > 0 ||
                        display_matrix_[pixel_idx * 3 + 1] > 0 ||
                        display_matrix_[pixel_idx * 3 + 2] > 0) {
                        display_matrix_[pixel_idx * 3] = region.r;
                        display_matrix_[pixel_idx * 3 + 1] = region.g;
                        display_matrix_[pixel_idx * 3 + 2] = region.b;
                    }
                }
            }
        }
    }
    
    void send_to_arduino()
    {
        if (!matrix_updated_) {
            return;
        }
        
        // TODO: Implement actual serial communication to Arduino
        // For now, just log that we would send data
        
        // Count non-black pixels
        int active_pixels = 0;
        for (size_t i = 0; i < display_matrix_.size(); i += 3) {
            if (display_matrix_[i] > 0 || display_matrix_[i+1] > 0 || display_matrix_[i+2] > 0) {
                active_pixels++;
            }
        }
        
        // Only log periodically to avoid spam
        static int log_counter = 0;
        if (log_counter++ % 10 == 0) {
            RCLCPP_INFO(this->get_logger(), "Would send matrix to Arduino: %d active pixels", 
                       active_pixels);
        }
        
        // Format for Arduino:
        // - Could send as compressed format (only send changed pixels)
        // - Could send as binary stream
        // - For now, just demonstrate the concept
        
        // Example serial write (uncomment when serial is available):
        // std::string data = format_matrix_for_arduino();
        // write_to_serial(data);
    }
    
    std::string format_matrix_for_arduino()
    {
        // Format matrix data for Arduino consumption
        // Simple format: "MATRIX:<height>x<width>:<RGB_data_as_hex>"
        std::ostringstream ss;
        ss << "MATRIX:" << matrix_height_ << "x" << matrix_width_ << ":";
        
        // Send as compressed format (only non-zero pixels)
        for (int row = 0; row < matrix_height_; row++) {
            for (int col = 0; col < matrix_width_; col++) {
                int idx = (row * matrix_width_ + col) * 3;
                uint8_t r = display_matrix_[idx];
                uint8_t g = display_matrix_[idx + 1];
                uint8_t b = display_matrix_[idx + 2];
                
                if (r > 0 || g > 0 || b > 0) {
                    ss << row << "," << col << "," 
                       << static_cast<int>(r) << ","
                       << static_cast<int>(g) << ","
                       << static_cast<int>(b) << ";";
                }
            }
        }
        
        return ss.str();
    }
    
    struct PaintRegion {
        double start_time;
        double end_time;
        uint8_t r, g, b;
    };
    
    // Member variables
    int matrix_height_;
    int matrix_width_;
    std::vector<uint8_t> display_matrix_;  // RGB data: R,G,B for each pixel
    std::vector<PaintRegion> paint_regions_;  // Stored paint overlays
    bool matrix_updated_ = false;
    
    std::string serial_port_;
    
    rclcpp::Subscription<std_msgs::msg::UInt8MultiArray>::SharedPtr matrix_subscription_;
    rclcpp::Subscription<std_msgs::msg::String>::SharedPtr paint_subscription_;
    rclcpp::TimerBase::SharedPtr send_timer_;
};

int main(int argc, char ** argv)
{
    rclcpp::init(argc, argv);
    auto node = std::make_shared<LEDMatrixWriter>();
    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;
}