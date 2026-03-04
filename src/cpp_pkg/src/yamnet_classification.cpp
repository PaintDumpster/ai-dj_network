#include <rclcpp/rclcpp.hpp>
#include <std_msgs/msg/string.hpp>
#include <std_msgs/msg/u_int8_multi_array.hpp>
#include <onnxruntime_cxx_api.h>
#include <vector>
#include <string>
#include <fstream>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <map>

class YAMNetClassifier : public rclcpp::Node
{
public:
    YAMNetClassifier() : Node("yamnet_classifier")
    {
        // Declare parameters
        this->declare_parameter("model_path", std::string(std::getenv("HOME")) + "/rosnetwork/models/yamnet.onnx");
        this->declare_parameter("class_names_path", std::string(std::getenv("HOME")) + "/rosnetwork/models/yamnet_class_names.txt");
        this->declare_parameter("model_name", "yamnet_default");
        this->declare_parameter("output_topic", "classification_results");
        this->declare_parameter("input_sample_rate", 44100);
        this->declare_parameter("yamnet_sample_rate", 16000);
        this->declare_parameter("waveform_file_check_interval", 1.0);
        this->declare_parameter("model_color_r", 255);  // Red channel for this model
        this->declare_parameter("model_color_g", 0);    // Green channel
        this->declare_parameter("model_color_b", 0);    // Blue channel
        
        model_path_ = this->get_parameter("model_path").as_string();
        class_names_path_ = this->get_parameter("class_names_path").as_string();
        model_name_ = this->get_parameter("model_name").as_string();
        output_topic_ = this->get_parameter("output_topic").as_string();
        input_sample_rate_ = this->get_parameter("input_sample_rate").as_int();
        yamnet_sample_rate_ = this->get_parameter("yamnet_sample_rate").as_int();
        model_color_r_ = this->get_parameter("model_color_r").as_int();
        model_color_g_ = this->get_parameter("model_color_g").as_int();
        model_color_b_ = this->get_parameter("model_color_b").as_int();
        
        // Load class names
        load_class_names();
        
        // Initialize ONNX Runtime
        try {
            initialize_onnx();
            RCLCPP_INFO(this->get_logger(), "ONNX Runtime initialized successfully");
        } catch (const std::exception& e) {
            RCLCPP_ERROR(this->get_logger(), "Failed to initialize ONNX: %s", e.what());
            rclcpp::shutdown();
            return;
        }
        
        // Publisher for classification results
        classification_pub_ = this->create_publisher<std_msgs::msg::String>(output_topic_, 10);
        
        // Publisher for LED paint commands
        paint_pub_ = this->create_publisher<std_msgs::msg::String>("led_paint_commands", 10);
        
        // Timer to check for new waveform files
        double check_interval = this->get_parameter("waveform_file_check_interval").as_double();
        timer_ = this->create_wall_timer(
            std::chrono::duration<double>(check_interval),
            std::bind(&YAMNetClassifier::check_for_waveform, this)
        );
        
        RCLCPP_INFO(this->get_logger(), "========================================");
        RCLCPP_INFO(this->get_logger(), "YAMNet Classifier: %s", model_name_.c_str());
        RCLCPP_INFO(this->get_logger(), "Model: %s", model_path_.c_str());
        RCLCPP_INFO(this->get_logger(), "Publishing to: %s", output_topic_.c_str());
        RCLCPP_INFO(this->get_logger(), "========================================");
    }
    
    ~YAMNetClassifier()
    {
        if (session_) {
            delete session_;
        }
        if (env_) {
            delete env_;
        }
    }

private:
    void initialize_onnx()
    {
        // Initialize ONNX Runtime environment
        env_ = new Ort::Env(ORT_LOGGING_LEVEL_WARNING, "YAMNetClassifier");
        
        // Session options
        Ort::SessionOptions session_options;
        session_options.SetIntraOpNumThreads(4);
        session_options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
        
        // Load model
        RCLCPP_INFO(this->get_logger(), "Loading model: %s", model_path_.c_str());
        session_ = new Ort::Session(*env_, model_path_.c_str(), session_options);
        
        // Get input/output info
        Ort::AllocatorWithDefaultOptions allocator;
        
        // Input info
        size_t num_input_nodes = session_->GetInputCount();
        RCLCPP_INFO(this->get_logger(), "Number of inputs: %zu", num_input_nodes);
        
        for (size_t i = 0; i < num_input_nodes; i++) {
            auto input_name = session_->GetInputNameAllocated(i, allocator);
            input_node_names_.push_back(input_name.get());
            RCLCPP_INFO(this->get_logger(), "Input %zu: %s", i, input_name.get());
        }
        
        // Output info
        size_t num_output_nodes = session_->GetOutputCount();
        RCLCPP_INFO(this->get_logger(), "Number of outputs: %zu", num_output_nodes);
        
        for (size_t i = 0; i < num_output_nodes; i++) {
            auto output_name = session_->GetOutputNameAllocated(i, allocator);
            output_node_names_.push_back(output_name.get());
            RCLCPP_INFO(this->get_logger(), "Output %zu: %s", i, output_name.get());
        }
    }
    
    void load_class_names()
    {
        std::ifstream file(class_names_path_);
        if (file.is_open()) {
            std::string line;
            while (std::getline(file, line)) {
                class_names_.push_back(line);
            }
            file.close();
            RCLCPP_INFO(this->get_logger(), "Loaded %zu class names", class_names_.size());
        } else {
            RCLCPP_WARN(this->get_logger(), "Could not load class names from: %s", 
                       class_names_path_.c_str());
            RCLCPP_WARN(this->get_logger(), "Will use class indices instead");
        }
    }
    
    void check_for_waveform()
    {
        // Check /tmp/ for new waveform files
        // This is a simple implementation - in production, consider using inotify or a subscription
        std::string pattern = "/tmp/waveform_";
        
        // For now, this is a placeholder - you could implement file monitoring
        // or use a service/topic-based approach instead
    }
    
    std::vector<float> load_waveform_from_csv(const std::string& filename)
    {
        std::vector<float> waveform;
        std::ifstream file(filename);
        
        if (!file.is_open()) {
            RCLCPP_ERROR(this->get_logger(), "Failed to open waveform file: %s", filename.c_str());
            return waveform;
        }
        
        std::string line;
        std::getline(file, line); // Skip header
        
        while (std::getline(file, line)) {
            size_t comma_pos = line.find(',');
            if (comma_pos != std::string::npos) {
                try {
                    float amplitude = std::stof(line.substr(comma_pos + 1));
                    waveform.push_back(amplitude);
                } catch (const std::exception& e) {
                    RCLCPP_WARN(this->get_logger(), "Failed to parse line: %s", line.c_str());
                }
            }
        }
        
        file.close();
        RCLCPP_INFO(this->get_logger(), "Loaded %zu samples from waveform", waveform.size());
        return waveform;
    }
    
    std::vector<float> resample_audio(const std::vector<float>& input, 
                                      int input_rate, int output_rate)
    {
        if (input_rate == output_rate) {
            return input;
        }
        
        // Simple linear interpolation resampling
        double ratio = static_cast<double>(output_rate) / input_rate;
        size_t output_length = static_cast<size_t>(input.size() * ratio);
        std::vector<float> output(output_length);
        
        for (size_t i = 0; i < output_length; i++) {
            double src_pos = i / ratio;
            size_t src_idx = static_cast<size_t>(src_pos);
            double frac = src_pos - src_idx;
            
            if (src_idx + 1 < input.size()) {
                output[i] = input[src_idx] * (1.0 - frac) + input[src_idx + 1] * frac;
            } else {
                output[i] = input[src_idx];
            }
        }
        
        RCLCPP_INFO(this->get_logger(), "Resampled from %d Hz to %d Hz (%zu -> %zu samples)",
                   input_rate, output_rate, input.size(), output.size());
        return output;
    }
    
    std::vector<float> preprocess_waveform(const std::vector<float>& waveform)
    {
        // Resample to 16kHz if needed
        std::vector<float> resampled = resample_audio(waveform, input_sample_rate_, yamnet_sample_rate_);
        
        // YAMNet expects 0.975 seconds (15600 samples at 16kHz)
        const size_t expected_length = 15600;
        
        std::vector<float> processed(expected_length, 0.0f);
        
        if (resampled.size() >= expected_length) {
            // Take the first 15600 samples
            std::copy(resampled.begin(), resampled.begin() + expected_length, processed.begin());
        } else {
            // Pad with zeros if too short
            std::copy(resampled.begin(), resampled.end(), processed.begin());
        }
        
        // Normalize to [-1, 1] range if needed
        float max_val = *std::max_element(processed.begin(), processed.end());
        float min_val = *std::min_element(processed.begin(), processed.end());
        float range = std::max(std::abs(max_val), std::abs(min_val));
        
        if (range > 1.0f) {
            for (auto& val : processed) {
                val /= range;
            }
        }
        
        return processed;
    }
    
    void classify_waveform(const std::string& waveform_file)
    {
        RCLCPP_INFO(this->get_logger(), "Classifying waveform: %s", waveform_file.c_str());
        
        // Load waveform from CSV
        std::vector<float> waveform = load_waveform_from_csv(waveform_file);
        if (waveform.empty()) {
            RCLCPP_ERROR(this->get_logger(), "Failed to load waveform");
            return;
        }
        
        // Preprocess
        std::vector<float> input_data = preprocess_waveform(waveform);
        
        // Run inference
        try {
            auto results = run_inference(input_data);
            publish_results(results, waveform_file);
        } catch (const std::exception& e) {
            RCLCPP_ERROR(this->get_logger(), "Inference failed: %s", e.what());
        }
    }
    
    std::vector<std::pair<int, float>> run_inference(const std::vector<float>& input_data)
    {
        // Create input tensor
        std::vector<int64_t> input_shape = {1, static_cast<int64_t>(input_data.size())};
        
        Ort::MemoryInfo memory_info = Ort::MemoryInfo::CreateCpu(
            OrtAllocatorType::OrtArenaAllocator, OrtMemType::OrtMemTypeDefault);
        
        Ort::Value input_tensor = Ort::Value::CreateTensor<float>(
            memory_info,
            const_cast<float*>(input_data.data()),
            input_data.size(),
            input_shape.data(),
            input_shape.size()
        );
        
        // Prepare input/output names
        std::vector<const char*> input_names;
        std::vector<const char*> output_names;
        
        for (const auto& name : input_node_names_) {
            input_names.push_back(name.c_str());
        }
        for (const auto& name : output_node_names_) {
            output_names.push_back(name.c_str());
        }
        
        // Run inference
        auto output_tensors = session_->Run(
            Ort::RunOptions{nullptr},
            input_names.data(),
            &input_tensor,
            1,
            output_names.data(),
            output_names.size()
        );
        
        // Process output (assuming first output is the class scores)
        float* output_data = output_tensors[0].GetTensorMutableData<float>();
        auto output_shape = output_tensors[0].GetTensorTypeAndShapeInfo().GetShape();
        
        size_t num_classes = output_shape[output_shape.size() - 1];
        size_t num_frames = 1;
        if (output_shape.size() > 1) {
            num_frames = output_shape[0];
        }
        
        // Store frame-by-frame predictions for time-based analysis
        frame_predictions_.clear();
        
        for (size_t frame = 0; frame < num_frames; frame++) {
            std::vector<std::pair<int, float>> frame_preds;
            size_t offset = frame * num_classes;
            
            for (size_t i = 0; i < num_classes; i++) {
                frame_preds.push_back({static_cast<int>(i), output_data[offset + i]});
            }
            
            // Sort by score
            std::sort(frame_preds.begin(), frame_preds.end(),
                     [](const auto& a, const auto& b) { return a.second > b.second; });
            
            frame_predictions_.push_back(frame_preds);
        }
        
        // Aggregate predictions across all frames
        std::vector<std::pair<int, float>> predictions;
        for (size_t i = 0; i < num_classes; i++) {
            predictions.push_back({static_cast<int>(i), output_data[i]});
        }
        
        // Sort by score (descending)
        std::sort(predictions.begin(), predictions.end(),
                 [](const auto& a, const auto& b) { return a.second > b.second; });
        
        return predictions;
    }
    
    void publish_results(const std::vector<std::pair<int, float>>& predictions,
                        const std::string& waveform_file)
    {
        // Get top 5 predictions
        const int top_k = 5;
        
        RCLCPP_INFO(this->get_logger(), "=== [%s] Classification Results for %s ===", 
                   model_name_.c_str(), waveform_file.c_str());
        
        std::string result_msg = "[" + model_name_ + "] Classification Results:\n";
        result_msg += "File: " + waveform_file + "\n";
        
        // Find time segments for top predictions
        std::map<int, std::vector<std::pair<double, double>>> class_time_segments;
        
        for (int i = 0; i < std::min(top_k, static_cast<int>(predictions.size())); i++) {
            int class_id = predictions[i].first;
            float score = predictions[i].second;
            
            std::string class_name = class_id < static_cast<int>(class_names_.size())
                ? class_names_[class_id]
                : "Class_" + std::to_string(class_id);
            
            RCLCPP_INFO(this->get_logger(), "  %d. %s: %.4f", i + 1, class_name.c_str(), score);
            
            result_msg += std::to_string(i + 1) + ". " + class_name + ": " + 
                         std::to_string(score) + "\n";
            
            // Find time segments where this class was dominant
            auto segments = find_time_segments_for_class(class_id, 0.3);  // threshold
            class_time_segments[class_id] = segments;
            
            // Log time segments
            if (!segments.empty()) {
                RCLCPP_INFO(this->get_logger(), "     Time segments: ");
                for (const auto& seg : segments) {
                    RCLCPP_INFO(this->get_logger(), "       %.2fs - %.2fs", seg.first, seg.second);
                }
            }
        }
        
        // Publish results
        auto msg = std_msgs::msg::String();
        msg.data = result_msg;
        classification_pub_->publish(msg);
        
        // Send paint commands for top prediction time segments
        if (!predictions.empty() && !class_time_segments.empty()) {
            int top_class = predictions[0].first;
            auto& segments = class_time_segments[top_class];
            
            for (const auto& segment : segments) {
                send_paint_command(segment.first, segment.second);
            }
        }
    }
    
    std::vector<std::pair<double, double>> find_time_segments_for_class(int class_id, float threshold)
    {
        std::vector<std::pair<double, double>> segments;
        
        if (frame_predictions_.empty()) {
            return segments;
        }
        
        // YAMNet produces predictions at ~96ms intervals
        const double frame_duration = 0.096;  // seconds per frame
        const double recording_duration = 30.0;  // 30 seconds total
        const double frames_per_second = 1.0 / frame_duration;
        
        bool in_segment = false;
        double segment_start = 0.0;
        
        for (size_t frame_idx = 0; frame_idx < frame_predictions_.size(); frame_idx++) {
            double time = frame_idx * frame_duration;
            const auto& frame_preds = frame_predictions_[frame_idx];
            
            // Check if this class is in top 3 for this frame
            bool class_active = false;
            for (int i = 0; i < std::min(3, static_cast<int>(frame_preds.size())); i++) {
                if (frame_preds[i].first == class_id && frame_preds[i].second > threshold) {
                    class_active = true;
                    break;
                }
            }
            
            if (class_active && !in_segment) {
                // Start new segment
                in_segment = true;
                segment_start = time;
            } else if (!class_active && in_segment) {
                // End current segment
                in_segment = false;
                segments.push_back({segment_start, time});
            }
        }
        
        // Close final segment if still open
        if (in_segment) {
            segments.push_back({segment_start, frame_predictions_.size() * frame_duration});
        }
        
        return segments;
    }
    
    void send_paint_command(double start_time, double end_time)
    {
        // Send command to paint LED matrix columns with model color
        // Format: "PAINT,start_time,end_time,R,G,B"
        std::string command = "PAINT," + 
                             std::to_string(start_time) + "," +
                             std::to_string(end_time) + "," +
                             std::to_string(model_color_r_) + "," +
                             std::to_string(model_color_g_) + "," +
                             std::to_string(model_color_b_);
        
        auto msg = std_msgs::msg::String();
        msg.data = command;
        paint_pub_->publish(msg);
        
        RCLCPP_INFO(this->get_logger(), "Sent paint command: %s", command.c_str());
    }
    
    // Public method to classify a waveform file (can be called via service in future)
public:
    void classify_file(const std::string& filename)
    {
        classify_waveform(filename);
    }

private:
    // ONNX Runtime objects
    Ort::Env* env_ = nullptr;
    Ort::Session* session_ = nullptr;
    std::vector<std::string> input_node_names_;
    std::vector<std::string> output_node_names_;
    
    // Parameters
    std::string model_path_;
    std::string class_names_path_;
    std::string model_name_;
    std::string output_topic_;
    int input_sample_rate_;
    int yamnet_sample_rate_;
    int model_color_r_;
    int model_color_g_;
    int model_color_b_;
    
    // Class names
    std::vector<std::string> class_names_;
    
    // Frame-by-frame predictions for time analysis
    std::vector<std::vector<std::pair<int, float>>> frame_predictions_;
    
    // ROS objects
    rclcpp::Publisher<std_msgs::msg::String>::SharedPtr classification_pub_;
    rclcpp::Publisher<std_msgs::msg::String>::SharedPtr paint_pub_;
    rclcpp::TimerBase::SharedPtr timer_;
};

int main(int argc, char** argv)
{
    rclcpp::init(argc, argv);
    auto node = std::make_shared<YAMNetClassifier>();
    
    // Example: Classify a specific file if provided as argument
    if (argc > 1) {
        std::string waveform_file = argv[1];
        RCLCPP_INFO(node->get_logger(), "Classifying provided file: %s", waveform_file.c_str());
        node->classify_file(waveform_file);
    }
    
    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;
}
