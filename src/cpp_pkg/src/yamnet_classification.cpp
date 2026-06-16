#include <rclcpp/rclcpp.hpp>
#include <std_msgs/msg/string.hpp>
#include <std_msgs/msg/u_int8_multi_array.hpp>
#include <std_srvs/srv/trigger.hpp>
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

        // Per-second confidence scores split by Pico board:
        //   pico_confidence_1 → seconds covering clusters 1-3 (Pico 1, ~18 s)
        //   pico_confidence_2 → seconds covering clusters 4-5 (Pico 2, ~12 s)
        // Each message: {"model":"...","color":[R,G,B],"per_second":[{"second":N,"scores":{...}},...]}
        pico_confidence_pub_1_ = this->create_publisher<std_msgs::msg::String>("pico_confidence_1", 10);
        pico_confidence_pub_2_ = this->create_publisher<std_msgs::msg::String>("pico_confidence_2", 10);
        
        // Service for triggering classification
        classify_service_ = this->create_service<std_srvs::srv::Trigger>(
            "classify_waveform_" + model_name_,
            std::bind(&YAMNetClassifier::handle_classify_request, this, std::placeholders::_1, std::placeholders::_2)
        );
        
        // Timer to check for new waveform files (disabled when using service-based control)
        // double check_interval = this->get_parameter("waveform_file_check_interval").as_double();
        // timer_ = this->create_wall_timer(
        //     std::chrono::duration<double>(check_interval),
        //     std::bind(&YAMNetClassifier::check_for_waveform, this)
        // );
        
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
        if (yamnet_session_) {
            delete yamnet_session_;
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
        
        // Load base YamNet model for embeddings
        std::string models_dir = model_path_.substr(0, model_path_.find_last_of("/\\"));
        std::string yamnet_base_path = models_dir + "/YamNet.onnx";
        
        RCLCPP_INFO(this->get_logger(), "Loading base YamNet model: %s", yamnet_base_path.c_str());
        yamnet_session_ = new Ort::Session(*env_, yamnet_base_path.c_str(), session_options);
        
        // Get YamNet input/output info
        Ort::AllocatorWithDefaultOptions allocator;
        
        size_t yamnet_inputs = yamnet_session_->GetInputCount();
        RCLCPP_INFO(this->get_logger(), "YamNet inputs: %zu", yamnet_inputs);
        for (size_t i = 0; i < yamnet_inputs; i++) {
            auto input_name = yamnet_session_->GetInputNameAllocated(i, allocator);
            yamnet_input_names_.push_back(input_name.get());
            RCLCPP_INFO(this->get_logger(), "  YamNet Input %zu: %s", i, input_name.get());
        }
        
        size_t yamnet_outputs = yamnet_session_->GetOutputCount();
        RCLCPP_INFO(this->get_logger(), "YamNet outputs: %zu", yamnet_outputs);
        for (size_t i = 0; i < yamnet_outputs; i++) {
            auto output_name = yamnet_session_->GetOutputNameAllocated(i, allocator);
            yamnet_output_names_.push_back(output_name.get());
            RCLCPP_INFO(this->get_logger(), "  YamNet Output %zu: %s", i, output_name.get());
        }
        
        // Load classification head model
        RCLCPP_INFO(this->get_logger(), "Loading classification head: %s", model_path_.c_str());
        session_ = new Ort::Session(*env_, model_path_.c_str(), session_options);
        
        // Get head input/output info
        size_t num_input_nodes = session_->GetInputCount();
        RCLCPP_INFO(this->get_logger(), "Head inputs: %zu", num_input_nodes);
        
        for (size_t i = 0; i < num_input_nodes; i++) {
            auto input_name = session_->GetInputNameAllocated(i, allocator);
            input_node_names_.push_back(input_name.get());
            RCLCPP_INFO(this->get_logger(), "  Head Input %zu: %s", i, input_name.get());
        }
        
        // Output info
        size_t num_output_nodes = session_->GetOutputCount();
        RCLCPP_INFO(this->get_logger(), "Head outputs: %zu", num_output_nodes);
        
        for (size_t i = 0; i < num_output_nodes; i++) {
            auto output_name = session_->GetOutputNameAllocated(i, allocator);
            output_node_names_.push_back(output_name.get());
            RCLCPP_INFO(this->get_logger(), "  Head Output %zu: %s", i, output_name.get());
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
    
    void handle_classify_request(
        const std::shared_ptr<std_srvs::srv::Trigger::Request> request,
        std::shared_ptr<std_srvs::srv::Trigger::Response> response)
    {
        (void)request;
        
        // Find the most recent waveform file
        std::string latest_file = find_latest_waveform_file();
        
        if (latest_file.empty()) {
            response->success = false;
            response->message = "No waveform file found in /tmp/";
            RCLCPP_WARN(this->get_logger(), "No waveform file found for classification");
            return;
        }
        
        RCLCPP_INFO(this->get_logger(), "Classifying: %s", latest_file.c_str());
        
        try {
            classify_waveform(latest_file);
            response->success = true;
            response->message = "Classification complete for " + latest_file;
        } catch (const std::exception& e) {
            response->success = false;
            response->message = std::string("Classification failed: ") + e.what();
            RCLCPP_ERROR(this->get_logger(), "Classification error: %s", e.what());
        }
    }
    
    std::string find_latest_waveform_file()
    {
        namespace fs = std::filesystem;
        std::string latest_file;
        std::filesystem::file_time_type latest_time;
        bool found = false;
        
        try {
            for (const auto& entry : fs::directory_iterator("/tmp")) {
                if (entry.is_regular_file()) {
                    std::string filename = entry.path().filename().string();
                    // Check if starts with "waveform_" and ends with ".csv"
                    bool is_csv = filename.size() >= 4 && 
                                  filename.substr(filename.size() - 4) == ".csv";
                    if (filename.find("waveform_") == 0 && is_csv) {
                        auto ftime = fs::last_write_time(entry);
                        if (!found || ftime > latest_time) {
                            latest_time = ftime;
                            latest_file = entry.path().string();
                            found = true;
                        }
                    }
                }
            }
        } catch (const std::exception& e) {
            RCLCPP_ERROR(this->get_logger(), "Error searching for waveform files: %s", e.what());
        }
        
        return latest_file;
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

        // YAMNet needs at least one analysis window (0.975s @ 16kHz = 15600 samples).
        // Anything longer is classified in full so events spread across the whole
        // recording are all seen, instead of only the first ~1 second.
        const size_t min_length = 15600;
        if (resampled.size() < min_length) {
            resampled.resize(min_length, 0.0f);
        }

        // Normalize to [-1, 1] range if needed
        float max_val = *std::max_element(resampled.begin(), resampled.end());
        float min_val = *std::min_element(resampled.begin(), resampled.end());
        float range = std::max(std::abs(max_val), std::abs(min_val));

        if (range > 1.0f) {
            for (auto& val : resampled) {
                val /= range;
            }
        }

        return resampled;
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
        Ort::MemoryInfo memory_info = Ort::MemoryInfo::CreateCpu(
            OrtAllocatorType::OrtArenaAllocator, OrtMemType::OrtMemTypeDefault);
        
        // ===== STAGE 1: Extract YamNet embeddings =====
        // Create input tensor for YamNet - expects 1D: [num_samples]
        std::vector<int64_t> audio_shape = {static_cast<int64_t>(input_data.size())};
        
        Ort::Value audio_tensor = Ort::Value::CreateTensor<float>(
            memory_info,
            const_cast<float*>(input_data.data()),
            input_data.size(),
            audio_shape.data(),
            audio_shape.size()
        );
        
        // Prepare YamNet input/output names
        std::vector<const char*> yamnet_input_names;
        std::vector<const char*> yamnet_output_names;
        
        for (const auto& name : yamnet_input_names_) {
            yamnet_input_names.push_back(name.c_str());
        }
        for (const auto& name : yamnet_output_names_) {
            yamnet_output_names.push_back(name.c_str());
        }
        
        // Run YamNet to get embeddings
        auto yamnet_outputs = yamnet_session_->Run(
            Ort::RunOptions{nullptr},
            yamnet_input_names.data(),
            &audio_tensor,
            1,
            yamnet_output_names.data(),
            yamnet_output_names.size()
        );
        
        // Extract embeddings - shape is [num_frames, 1024], one frame per ~0.48s of audio
        float* embeddings_data = yamnet_outputs[0].GetTensorMutableData<float>();
        auto embeddings_shape = yamnet_outputs[0].GetTensorTypeAndShapeInfo().GetShape();
        size_t num_frames = embeddings_shape[0];
        size_t embedding_size = embeddings_shape[1];

        RCLCPP_INFO(this->get_logger(), "YamNet embeddings extracted: [%zu frames, %zu dims]",
                   num_frames, embedding_size);

        // Remember how long each frame spans in real time, so later timeline buckets
        // can map frame index back to seconds into the recording.
        double audio_duration_seconds = static_cast<double>(input_data.size()) / yamnet_sample_rate_;
        frame_duration_ = num_frames > 0 ? audio_duration_seconds / static_cast<double>(num_frames) : 0.0;

        // ===== STAGE 2: Run classification head on every frame in one batched call =====
        // The head model has a dynamic batch dimension, so all frames can be classified
        // together instead of averaging embeddings into a single frame beforehand.
        std::vector<int64_t> head_input_shape = {static_cast<int64_t>(num_frames), static_cast<int64_t>(embedding_size)};

        Ort::Value embedding_tensor = Ort::Value::CreateTensor<float>(
            memory_info,
            embeddings_data,
            num_frames * embedding_size,
            head_input_shape.data(),
            head_input_shape.size()
        );

        // Prepare classification head input/output names
        std::vector<const char*> head_input_names;
        std::vector<const char*> head_output_names;

        for (const auto& name : input_node_names_) {
            head_input_names.push_back(name.c_str());
        }
        for (const auto& name : output_node_names_) {
            head_output_names.push_back(name.c_str());
        }

        // Run classification head
        auto output_tensors = session_->Run(
            Ort::RunOptions{nullptr},
            head_input_names.data(),
            &embedding_tensor,
            1,
            head_output_names.data(),
            head_output_names.size()
        );

        // Process output (class scores)
        float* output_data = output_tensors[0].GetTensorMutableData<float>();
        auto output_shape = output_tensors[0].GetTensorTypeAndShapeInfo().GetShape();

        size_t num_classes = output_shape[output_shape.size() - 1];
        size_t output_frames = 1;
        if (output_shape.size() > 1) {
            output_frames = output_shape[0];
        }

        // Store frame-by-frame predictions for the timeline and LED-matrix time segments
        frame_predictions_.clear();

        for (size_t frame = 0; frame < output_frames; frame++) {
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

        // Aggregate predictions across all frames (mean score per class over the whole recording)
        std::vector<float> avg_scores(num_classes, 0.0f);
        for (size_t frame = 0; frame < output_frames; frame++) {
            size_t offset = frame * num_classes;
            for (size_t i = 0; i < num_classes; i++) {
                avg_scores[i] += output_data[offset + i];
            }
        }
        if (output_frames > 0) {
            for (auto& s : avg_scores) s /= static_cast<float>(output_frames);
        }

        std::vector<std::pair<int, float>> predictions;
        for (size_t i = 0; i < num_classes; i++) {
            predictions.push_back({static_cast<int>(i), avg_scores[i]});
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
        
        // Append a time-bucketed timeline so downstream consumers (the LLM node) can
        // describe how the soundscape changed across the recording, not just the
        // single dominant label.
        result_msg += build_timeline();

        // Publish classification results
        auto msg = std_msgs::msg::String();
        msg.data = result_msg;
        classification_pub_->publish(msg);

        // Publish per-second confidence scores for Pico board color-coding
        publish_pico_confidence();

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
        
        if (frame_predictions_.empty() || frame_duration_ <= 0.0) {
            return segments;
        }

        bool in_segment = false;
        double segment_start = 0.0;

        for (size_t frame_idx = 0; frame_idx < frame_predictions_.size(); frame_idx++) {
            double time = frame_idx * frame_duration_;
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
            segments.push_back({segment_start, frame_predictions_.size() * frame_duration_});
        }
        
        return segments;
    }

    // Build a "Timeline:\n0-3s: Label (0.42)\n..." section summarizing the dominant
    // class in fixed-size buckets across the whole recording. Distinct from the
    // "N. Label: score" lines above it, so existing parsers of the top-k list are
    // unaffected by appending this.
    std::string build_timeline(int bucket_seconds = 3)
    {
        if (frame_predictions_.empty() || frame_duration_ <= 0.0 || class_names_.empty()) {
            return "";
        }

        size_t num_classes = class_names_.size();
        double total_duration = frame_predictions_.size() * frame_duration_;
        int total_seconds = static_cast<int>(std::ceil(total_duration));
        if (total_seconds <= 0) {
            return "";
        }

        std::string timeline = "Timeline:\n";

        for (int bucket_start = 0; bucket_start < total_seconds; bucket_start += bucket_seconds) {
            int bucket_end = std::min(bucket_start + bucket_seconds, total_seconds);

            std::vector<double> avg(num_classes, 0.0);
            size_t count = 0;
            for (size_t f = 0; f < frame_predictions_.size(); f++) {
                double t = f * frame_duration_;
                if (t < bucket_start || t >= bucket_end) continue;
                for (const auto& pred : frame_predictions_[f]) {
                    if (pred.first >= 0 && static_cast<size_t>(pred.first) < num_classes)
                        avg[pred.first] += pred.second;
                }
                count++;
            }
            if (count == 0) continue;
            for (auto& s : avg) s /= static_cast<double>(count);

            size_t top_class = 0;
            double top_score = avg[0];
            for (size_t c = 1; c < num_classes; c++) {
                if (avg[c] > top_score) {
                    top_score = avg[c];
                    top_class = c;
                }
            }

            timeline += std::to_string(bucket_start) + "-" + std::to_string(bucket_end) + "s: " +
                        class_names_[top_class] + " (" + std::to_string(top_score) + ")\n";
        }

        return timeline;
    }

    void publish_pico_confidence()
    {
        // Aggregate per-frame predictions (~0.096 s each) into 1-second bins,
        // then split between the two Pico boards by time:
        //   Pico 1 — clusters 1-3 → first  3/5 of total seconds (~18 s for 30 s recording)
        //   Pico 2 — clusters 4-5 → last   2/5 of total seconds (~12 s)
        const size_t num_classes = class_names_.size();
        if (frame_predictions_.empty() || num_classes == 0 || frame_duration_ <= 0.0) return;

        double total_duration = frame_predictions_.size() * frame_duration_;
        int total_seconds     = static_cast<int>(std::ceil(total_duration));

        // Boundary: Pico 1 takes the first 3 clusters out of 5
        int pico1_seconds = (total_seconds * 3 + 4) / 5;  // ceiling of 3/5 * total

        std::string color_str = "[" + std::to_string(model_color_r_) + "," +
                                      std::to_string(model_color_g_) + "," +
                                      std::to_string(model_color_b_) + "]";
        std::string header = "{\"model\":\"" + model_name_ + "\","
                             "\"color\":" + color_str + ","
                             "\"per_second\":[";

        // Build per-second JSON entries
        auto second_json = [&](int sec) -> std::string {
            size_t frame_start = static_cast<size_t>(sec / frame_duration_);
            size_t frame_end   = static_cast<size_t>((sec + 1) / frame_duration_);
            frame_end = std::min(frame_end, frame_predictions_.size());

            std::vector<double> avg(num_classes, 0.0);
            size_t count = 0;
            for (size_t f = frame_start; f < frame_end; f++) {
                for (const auto& pred : frame_predictions_[f]) {
                    if (pred.first >= 0 && static_cast<size_t>(pred.first) < num_classes)
                        avg[pred.first] += pred.second;
                }
                count++;
            }
            if (count > 0) for (auto& s : avg) s /= count;

            std::string entry = "{\"second\":" + std::to_string(sec) + ",\"scores\":{";
            for (size_t c = 0; c < num_classes; c++) {
                if (c > 0) entry += ",";
                entry += "\"" + class_names_[c] + "\":" + std::to_string(static_cast<float>(avg[c]));
            }
            return entry + "}}";
        };

        // Pico 1 message (seconds 0 .. pico1_seconds-1)
        std::string json1 = header;
        for (int s = 0; s < pico1_seconds; s++) {
            if (s > 0) json1 += ",";
            json1 += second_json(s);
        }
        json1 += "]}";

        // Pico 2 message (seconds pico1_seconds .. total_seconds-1)
        std::string json2 = header;
        for (int s = pico1_seconds; s < total_seconds; s++) {
            if (s > pico1_seconds) json2 += ",";
            json2 += second_json(s);
        }
        json2 += "]}";

        auto msg1 = std_msgs::msg::String(); msg1.data = json1;
        auto msg2 = std_msgs::msg::String(); msg2.data = json2;
        pico_confidence_pub_1_->publish(msg1);
        pico_confidence_pub_2_->publish(msg2);
        RCLCPP_INFO(this->get_logger(),
                    "Published pico_confidence_1/2 for %s (Pico1: 0-%ds, Pico2: %d-%ds)",
                    model_name_.c_str(), pico1_seconds - 1, pico1_seconds, total_seconds - 1);
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
    Ort::Session* yamnet_session_ = nullptr;  // Base YamNet model
    Ort::Session* session_ = nullptr;         // Classification head
    std::vector<std::string> yamnet_input_names_;
    std::vector<std::string> yamnet_output_names_;
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
    // Seconds spanned by each entry in frame_predictions_, computed per classification run
    double frame_duration_ = 0.0;
    
    // ROS objects
    rclcpp::Publisher<std_msgs::msg::String>::SharedPtr classification_pub_;
    rclcpp::Publisher<std_msgs::msg::String>::SharedPtr paint_pub_;
    rclcpp::Publisher<std_msgs::msg::String>::SharedPtr pico_confidence_pub_1_;
    rclcpp::Publisher<std_msgs::msg::String>::SharedPtr pico_confidence_pub_2_;
    rclcpp::TimerBase::SharedPtr timer_;
    rclcpp::Service<std_srvs::srv::Trigger>::SharedPtr classify_service_;
};

int main(int argc, char** argv)
{
    rclcpp::init(argc, argv);
    auto node = std::make_shared<YAMNetClassifier>();
    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;
}
