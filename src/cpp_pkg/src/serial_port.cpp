#include "cpp_pkg/serial_port.hpp"
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <cstring>
#include <stdexcept>

SerialPort::SerialPort(const std::string& port_name, int baud_rate)
{
    fd_ = open(port_name.c_str(), O_RDWR | O_NOCTTY);
    if (fd_ < 0) {
        throw std::runtime_error("Failed to open serial port " + port_name);
    }

    struct termios tty;
    memset(&tty, 0, sizeof(tty));

    if (tcgetattr(fd_, &tty) != 0){
        throw std::runtime_error("Error from tcgetattr");
    }
    
    cfsetospeed(&tty, baud_rate);
    cfsetispeed(&tty, baud_rate);

    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8; // 8-bit chars
    tty.c_cflag &= ~PARENB; // no parity
    tty.c_cflag &= ~CSTOPB; // 1 stop bit
    tty.c_cflag &= ~CRTSCTS; // no hardware flow control
    tty.c_cflag |= CREAD | CLOCAL; // turn on READ & ignore ctrl lines

    tty.c_lflag = 0;
    tty.c_oflag = 0;
    tty.c_iflag = 0;

    tty.c_cc[VMIN] = 0; // read doesn't block
    tty.c_cc[VTIME] = 10; // 1 second read timeout

    tcsetattr(fd_, TCSANOW, &tty);
}

SerialPort::~SerialPort()
{
    if (fd_ >= 0) {
        close(fd_);
    }
}

std::string SerialPort::readline()
{
    std::string line;
    char buf[1];
    while (true) {
        int n = ::read(fd_, buf, 1);
        if (n > 0) {
            if (buf[0] == '\n') {
                break;
            }
            line += buf[0];
        } else {
            break; // timeout or error;
        }
    }
    return line;
}

void SerialPort::writeline(const std::string& data)
{
    ::write(fd_, data.c_str(), data.length());
}