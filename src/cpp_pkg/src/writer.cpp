#include <cstdint>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <rclcpp/rclcpp.hpp>
#include <std_msgs/msg/float32_multi_array.hpp>
#include <std_msgs/msg/string.hpp>
#include "cpp_pkg/serial_port.hpp"

// ─── Protocol constants ───────────────────────────────────────────────────────
static constexpr int    MATRIX_ROWS      = 74;
static constexpr int    TOTAL_COLS       = 75;
static constexpr int    RECORDING_SEC    = 30;
static constexpr int    INTERVAL_MS      = 400;   // 30000ms / 75 cols
static constexpr uint8_t SYNC0           = 0xAA;
static constexpr uint8_t SYNC1           = 0x55;
static constexpr uint8_t CMD_WAVEFORM    = 0x01;
static constexpr uint8_t CMD_COLORIZE    = 0x02;
static constexpr uint8_t CMD_CLEAR       = 0x03;
static constexpr uint8_t CMD_RESET       = 0x04;

// ─── JSON helpers (no external library) ──────────────────────────────────────

// Extract string value after a key like "model": or "model" :
static std::string json_string_field(const std::string& json, const std::string& key) {
    std::string needle = "\"" + key + "\"";
    size_t k = json.find(needle);
    if (k == std::string::npos) return {};
    size_t colon = json.find(':', k + needle.size());
    if (colon == std::string::npos) return {};
    size_t q1 = json.find('"', colon + 1);
    if (q1 == std::string::npos) return {};
    size_t q2 = json.find('"', q1 + 1);
    if (q2 == std::string::npos) return {};
    return json.substr(q1 + 1, q2 - q1 - 1);
}

// Extract integer array [R,G,B] from "color": [R,G,B]
static std::vector<int> json_int_array(const std::string& json, const std::string& key) {
    std::string needle = "\"" + key + "\"";
    size_t k = json.find(needle);
    if (k == std::string::npos) return {};
    size_t colon = json.find(':', k + needle.size());
    if (colon == std::string::npos) return {};
    size_t arr_open  = json.find('[', colon);
    size_t arr_close = json.find(']', arr_open + 1);
    if (arr_open == std::string::npos || arr_close == std::string::npos) return {};
    std::string content = json.substr(arr_open + 1, arr_close - arr_open - 1);
    std::istringstream ss(content);
    std::string tok;
    std::vector<int> result;
    while (std::getline(ss, tok, ',')) {
        try { result.push_back(std::stoi(tok)); } catch (...) {}
    }
    return result;
}

// Parse per_second max confidence scores, keyed by second index.
// JSON format: "per_second": [{"second": N, "scores": {"Label": 0.87, ...}}, ...]
static std::map<int, float> parse_max_scores(const std::string& json) {
    std::map<int, float> result;
    size_t pos = 0;

    while (true) {
        size_t sec_key = json.find("\"second\"", pos);
        if (sec_key == std::string::npos) break;

        // Parse second index
        size_t colon = json.find(':', sec_key + 8);
        if (colon == std::string::npos) break;
        size_t num_start = colon + 1;
        while (num_start < json.size() && json[num_start] == ' ') num_start++;
        int second = 0;
        try {
            size_t consumed;
            second = std::stoi(json.substr(num_start), &consumed);
            pos = num_start + consumed;
        } catch (...) { pos = sec_key + 8; continue; }

        // Find scores object
        size_t scores_key = json.find("\"scores\"", pos);
        if (scores_key == std::string::npos) break;
        size_t brace_open  = json.find('{', scores_key + 8);
        if (brace_open == std::string::npos) break;
        size_t brace_close = json.find('}', brace_open + 1);
        if (brace_close == std::string::npos) break;

        // Find max float value inside scores object
        float max_score = 0.0f;
        size_t p = brace_open;
        while (p < brace_close) {
            size_t val_colon = json.find(": ", p);
            if (val_colon == std::string::npos || val_colon >= brace_close) break;
            size_t val_start = val_colon + 2;
            if (val_start >= brace_close) break;
            try {
                size_t consumed;
                float v = std::stof(json.substr(val_start, brace_close - val_start), &consumed);
                if (v > max_score) max_score = v;
                p = val_start + consumed;
            } catch (...) { p = val_colon + 2; }
        }
        result[second] = max_score;
        pos = brace_close + 1;
    }
    return result;
}

// ─── PicoWriter node ──────────────────────────────────────────────────────────
class PicoWriter : public rclcpp::Node {
public:
    PicoWriter() : Node("pico_writer") {
        this->declare_parameter("serial_port",   std::string("/dev/ttyACM1"));
        this->declare_parameter("pico_id",       1);
        this->declare_parameter("n_cols",        45);
        this->declare_parameter("global_offset", 0);

        serial_port_   = this->get_parameter("serial_port").as_string();
        pico_id_       = this->get_parameter("pico_id").as_int();
        n_cols_        = this->get_parameter("n_cols").as_int();
        global_offset_ = this->get_parameter("global_offset").as_int();

        // start_delay: Pico 1 offset=0 → 0ms; Pico 2 offset=45 → 45×400=18000ms
        start_delay_ms_ = static_cast<uint16_t>(global_offset_ * INTERVAL_MS);
        amplitudes_.assign(static_cast<size_t>(n_cols_), 0);

        try {
            serial_ = std::make_unique<SerialPort>(serial_port_, 115200);
            RCLCPP_INFO(this->get_logger(),
                "Pico %d: serial opened %s (n_cols=%d, offset=%d, start_delay=%dms)",
                pico_id_, serial_port_.c_str(), n_cols_, global_offset_, start_delay_ms_);
        } catch (const std::exception& e) {
            RCLCPP_ERROR(this->get_logger(), "Failed to open %s: %s", serial_port_.c_str(), e.what());
            throw;
        }

        // Topic subscriptions
        std::string wf_topic   = "pico_waveform_"   + std::to_string(pico_id_);
        std::string conf_topic = "pico_confidence_" + std::to_string(pico_id_);

        waveform_sub_ = this->create_subscription<std_msgs::msg::Float32MultiArray>(
            wf_topic, 10,
            [this](const std_msgs::msg::Float32MultiArray::SharedPtr m) { on_waveform(m); });

        confidence_sub_ = this->create_subscription<std_msgs::msg::String>(
            conf_topic, 10,
            [this](const std_msgs::msg::String::SharedPtr m) { on_confidence(m); });

        colorize_sub_ = this->create_subscription<std_msgs::msg::String>(
            "colorize_command", 10,
            [this](const std_msgs::msg::String::SharedPtr m) { on_colorize(m); });
    }

private:
    // ── Subscription handlers ──────────────────────────────────────────────────

    void on_waveform(const std_msgs::msg::Float32MultiArray::SharedPtr msg) {
        size_t expected = static_cast<size_t>(MATRIX_ROWS * n_cols_);
        if (msg->data.size() != expected) {
            RCLCPP_WARN(this->get_logger(),
                "Waveform size mismatch: expected %zu got %zu", expected, msg->data.size());
            return;
        }
        // Extract bar height per column: count pixels above threshold
        for (int c = 0; c < n_cols_; c++) {
            int height = 0;
            for (int r = 0; r < MATRIX_ROWS; r++) {
                if (msg->data[static_cast<size_t>(r * n_cols_ + c)] > 0.01f) height++;
            }
            amplitudes_[static_cast<size_t>(c)] =
                static_cast<uint8_t>(std::min(height, MATRIX_ROWS));
        }
        send_waveform();
    }

    void on_confidence(const std_msgs::msg::String::SharedPtr msg) {
        // Cache JSON by model name for use when colorize command arrives
        std::string model = json_string_field(msg->data, "model");
        if (!model.empty()) {
            confidence_cache_[model] = msg->data;
            RCLCPP_DEBUG(this->get_logger(), "Cached confidence for model: %s", model.c_str());
        }
    }

    void on_colorize(const std_msgs::msg::String::SharedPtr msg) {
        const std::string& model = msg->data;
        if (model.empty()) {
            send_clear();
            return;
        }
        auto it = confidence_cache_.find(model);
        if (it == confidence_cache_.end()) {
            RCLCPP_WARN(this->get_logger(), "No cached confidence for model: %s", model.c_str());
            return;
        }

        // Extract model color
        uint8_t r = 255, g = 255, b = 255;
        auto rgb = json_int_array(it->second, "color");
        if (rgb.size() >= 3) {
            r = static_cast<uint8_t>(rgb[0]);
            g = static_cast<uint8_t>(rgb[1]);
            b = static_cast<uint8_t>(rgb[2]);
        }

        // Map per-second confidence to per-column
        auto scores = parse_max_scores(it->second);
        std::vector<uint8_t> col_conf(static_cast<size_t>(n_cols_));
        for (int c = 0; c < n_cols_; c++) {
            int global_col = c + global_offset_;
            int second     = global_col * RECORDING_SEC / TOTAL_COLS;
            float score    = 0.0f;
            auto sit = scores.find(second);
            if (sit != scores.end()) score = sit->second;
            col_conf[static_cast<size_t>(c)] = static_cast<uint8_t>(score * 255.0f);
        }

        send_colorize(r, g, b, col_conf);
    }

    // ── Packet senders ─────────────────────────────────────────────────────────

    void send_waveform() {
        // Payload: INTERVAL_MS(2) START_DELAY(2) N_COLS(1) AMPLITUDES(N_COLS)
        uint16_t payload_len = 5 + static_cast<uint16_t>(n_cols_);
        std::vector<uint8_t> pkt;
        pkt.reserve(5 + payload_len);
        push_header(pkt, CMD_WAVEFORM, payload_len);
        pkt.push_back((INTERVAL_MS >> 8) & 0xFF);
        pkt.push_back( INTERVAL_MS       & 0xFF);
        pkt.push_back((start_delay_ms_ >> 8) & 0xFF);
        pkt.push_back( start_delay_ms_        & 0xFF);
        pkt.push_back(static_cast<uint8_t>(n_cols_));
        for (uint8_t a : amplitudes_) pkt.push_back(a);
        write_serial(pkt);
        RCLCPP_INFO(this->get_logger(),
            "Pico %d: WAVEFORM sent (%d cols, delay %dms)", pico_id_, n_cols_, start_delay_ms_);
    }

    void send_colorize(uint8_t r, uint8_t g, uint8_t b,
                       const std::vector<uint8_t>& col_conf) {
        // Payload: R G B N_COLS CONFIDENCE(N_COLS)
        uint16_t payload_len = 4 + static_cast<uint16_t>(col_conf.size());
        std::vector<uint8_t> pkt;
        pkt.reserve(5 + payload_len);
        push_header(pkt, CMD_COLORIZE, payload_len);
        pkt.push_back(r); pkt.push_back(g); pkt.push_back(b);
        pkt.push_back(static_cast<uint8_t>(col_conf.size()));
        for (uint8_t c : col_conf) pkt.push_back(c);
        write_serial(pkt);
        RCLCPP_INFO(this->get_logger(),
            "Pico %d: COLORIZE sent (RGB %d,%d,%d)", pico_id_, r, g, b);
    }

    void send_clear() {
        std::vector<uint8_t> pkt;
        push_header(pkt, CMD_CLEAR, 0);
        write_serial(pkt);
        RCLCPP_INFO(this->get_logger(), "Pico %d: CLEAR sent", pico_id_);
    }

    void send_reset() {
        std::vector<uint8_t> pkt;
        push_header(pkt, CMD_RESET, 0);
        write_serial(pkt);
        RCLCPP_INFO(this->get_logger(), "Pico %d: RESET sent", pico_id_);
    }

    static void push_header(std::vector<uint8_t>& pkt, uint8_t cmd, uint16_t len) {
        pkt.push_back(SYNC0);
        pkt.push_back(SYNC1);
        pkt.push_back(cmd);
        pkt.push_back((len >> 8) & 0xFF);
        pkt.push_back(len & 0xFF);
    }

    void write_serial(const std::vector<uint8_t>& pkt) {
        if (!serial_) {
            RCLCPP_WARN_ONCE(this->get_logger(), "Serial not available");
            return;
        }
        try {
            serial_->write_bytes(pkt.data(), pkt.size());
        } catch (const std::exception& e) {
            RCLCPP_ERROR(this->get_logger(), "Serial write error: %s", e.what());
        }
    }

    // ── Members ────────────────────────────────────────────────────────────────
    std::string  serial_port_;
    int          pico_id_;
    int          n_cols_;
    int          global_offset_;
    uint16_t     start_delay_ms_;

    std::vector<uint8_t>              amplitudes_;
    std::map<std::string, std::string> confidence_cache_;
    std::unique_ptr<SerialPort>       serial_;

    rclcpp::Subscription<std_msgs::msg::Float32MultiArray>::SharedPtr waveform_sub_;
    rclcpp::Subscription<std_msgs::msg::String>::SharedPtr            confidence_sub_;
    rclcpp::Subscription<std_msgs::msg::String>::SharedPtr            colorize_sub_;
};

int main(int argc, char** argv) {
    rclcpp::init(argc, argv);
    auto node = std::make_shared<PicoWriter>();
    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;
}
