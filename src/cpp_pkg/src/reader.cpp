#include <rclcpp/rclcpp.hpp>
#include <std_msgs/msg/string.hpp>
#include "cpp_pkg/serial_port.hpp"

class reader : public rclcpp::Node
{
public:
    reader() : Node("reader")
    {
        this->declare_parameter("port", "/dev/ttyACM0");
        this->declare_parameter("baud_rate", 9600);

        std::string port_name = this->get_parameter("port").as_string();
        int baud_rate = this->get_parameter("baud_rate").as_int();

        try {
            serial_ = std::make_unique<SerialPort>(port_name, baud_rate);
            RCLCPP_INFO(this->get_logger(), "Serial port opened: %s at %d baud", port_name.c_str(), baud_rate);
        } catch (const std::exception& e) {
            RCLCPP_ERROR(this->get_logger(), "Failed to open serial port: %s", e.what());
            rclcpp::shutdown();
            return;
        }

        publisher_ = this->create_publisher<std_msgs::msg::String>("arduino_data", 10);
        state_control_publisher_ = this->create_publisher<std_msgs::msg::String>("state_control", 10);
        timer_ = this->create_wall_timer(
            std::chrono::milliseconds(100),
            std::bind(&reader::read_and_publish, this)
        );
    }
private:
    std::string trim(const std::string& str)
    {
        // Remove leading and trailing whitespace (including \r and \n)
        size_t start = str.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) return "";
        size_t end = str.find_last_not_of(" \t\r\n");
        return str.substr(start, end - start + 1);
    }
    
    void read_and_publish()
    {
        std::string data = serial_->readline();
        if (!data.empty()) {
            // Strip whitespace and carriage returns
            data = trim(data);
            
            if (data.empty()) return;
            
            auto message = std_msgs::msg::String();
            message.data = data;
            
            // Button 11 is for state control, publish to separate topic
            if (data == "11") {
                state_control_publisher_->publish(message);
                RCLCPP_INFO(this->get_logger(), "State control button pressed");
            } else {
                // Buttons 1-10 are for sounds
                publisher_->publish(message);
                RCLCPP_INFO(this->get_logger(), "Published: '%s'", message.data.c_str());
            }
        }
    }

    std::unique_ptr<SerialPort> serial_;
    rclcpp::Publisher<std_msgs::msg::String>::SharedPtr publisher_;
    rclcpp::Publisher<std_msgs::msg::String>::SharedPtr state_control_publisher_;
    rclcpp::TimerBase::SharedPtr timer_;
};

int main(int argc, char ** argv)
{
    rclcpp::init(argc, argv);
    auto node = std::make_shared<reader>();
    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;
}