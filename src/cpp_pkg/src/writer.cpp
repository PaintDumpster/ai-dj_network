#include <iostream>
#include <rclcpp/rclcpp.hpp>

int main(int argc, char ** argv)
{
    rclcpp::init(argc, argv);
    // node code
    rclcpp::shutdown();
    return {0};
}