#include "serial_port.hpp"

#ifdef OS_CYGWIN
SerialPort::SerialPort(std::shared_ptr<BoundedBuffer<char>> byte_buffer_) :
    windows_port{byte_buffer_}
{
}
#elif OS_LINUX
SerialPort::SerialPort(std::shared_ptr<BoundedBuffer<char>> byte_buffer_) :
    linux_port{byte_buffer_}
{
}
#endif

SerialPort::~SerialPort()
{
#ifdef OS_CYGWIN
#elif OS_LINUX
#endif
}

std::vector<std::string> SerialPort::find_ports() const
{
#ifdef OS_CYGWIN
    return windows_port.find_ports();
#elif OS_LINUX
    return linux_port.find_ports();
#endif
}

bool SerialPort::is_open() const
{
#ifdef OS_CYGWIN
    return windows_port.is_open();
#elif OS_LINUX
    return linux_port.is_open();
#endif
}

bool SerialPort::is_reading() const
{
#ifdef OS_CYGWIN
    return windows_port.is_reading();
#elif OS_LINUX
    return linux_port.is_reading();
#endif
}

std::string SerialPort::get_port_name() const
{
#ifdef OS_CYGWIN
    return windows_port.get_port_name();
#elif OS_LINUX
    return linux_port.get_port_name();
#endif
}

bool SerialPort::open(const std::string& port)
{
#ifdef OS_CYGWIN
    return windows_port.open(port);
#elif OS_LINUX
    return linux_port.open(port);
#endif
}

#ifdef OS_CYGWIN
bool SerialPort::config()
{
    return windows_port.config();
}
#elif OS_LINUX
void SerialPort::config(const LinuxSerialPortConfig& cfg)
{
    return linux_port.config(cfg);
}
#endif

void SerialPort::start_reading()
{
#ifdef OS_CYGWIN
    windows_port.start_reading();
#elif OS_LINUX
    linux_port.start_reading();
#endif
}

void SerialPort::stop_reading()
{
#ifdef OS_CYGWIN
    windows_port.stop_reading();
#elif OS_LINUX
    linux_port.stop_reading();
#endif
}
