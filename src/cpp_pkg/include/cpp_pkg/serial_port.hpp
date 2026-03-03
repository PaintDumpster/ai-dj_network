#ifndef CPP_PKG_SERIAL_PORT_HPP
#define CPP_PKG_SERIAL_PORT_HPP

#include <string>

class SerialPort
{
    public:
        SerialPort(const std::string& port_name, int baud_rate);
        ~SerialPort();

        std::string readline();
        void writeline(const std::string& data);

    private:
        int fd_;
};

#endif // CPP_PKG_SERIAL_PORT_HPP_