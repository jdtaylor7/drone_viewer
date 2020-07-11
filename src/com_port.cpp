#include "com_port.hpp"

#include <array>
#include <algorithm>

ComPort::ComPort(std::size_t packet_len_,
        char packet_start_symbol_,
        char packet_stop_symbol) :
    packet_len(packet_len_),
    packet_start_symbol(packet_start_symbol_),
    packet_stop_symbol(packet_stop_symbol)
{
#ifdef OS_LINUX
    libusb_init(NULL);
#endif
}

ComPort::~ComPort()
{
#ifdef OS_CYGWIN
    CloseHandle(handle);
#elif OS_LINUX
    // TODO
#endif
}

bool ComPort::init()
{
    if (!connected)
    {
        std::cout << "Cannot initialize port without a connection.\n";
        return false;
    }

    if (initialized)
    {
        std::cout << "Port has already been initialized.\n";
        return false;
    }

#ifdef OS_CYGWIN
    /*
     * Set COM mask. Register event for receive buffer getting data.
     */
    if (!SetCommMask(handle, EV_RXCHAR))
    {
        std::cout << "Error setting COM mask: " << GetLastError() << '\n';
        return false;
    }

    /*
     * Set com port paramaters.
     */
    DCB dcb = {0};
    dcb.DCBlength = sizeof(DCB);

    if (!GetCommState(handle, &dcb))
    {
        std::cout << "Error getting comm state: " << GetLastError() << '\n';
        return false;
    }

    dcb.BaudRate = 9600;
    dcb.ByteSize = 8;
    dcb.Parity = NOPARITY;
    dcb.StopBits = ONESTOPBIT;

    if (!SetCommState(handle, &dcb))
    {
        std::cout << "Error setting comm state: " << GetLastError() << '\n';
        return false;
    }

    /*
     * Set timeouts.
     */
    COMMTIMEOUTS com_timeouts;
    com_timeouts.ReadIntervalTimeout = MAXDWORD;
    com_timeouts.ReadTotalTimeoutMultiplier = 0;
    com_timeouts.ReadTotalTimeoutConstant = 0;

    if (!SetCommTimeouts(handle, &com_timeouts))
    {
        std::cout << "Error settings timeouts: " << GetLastError() << '\n';
        return false;
    }
#endif

    initialized = true;
    return true;
}

bool ComPort::start()
{
    if (is_reading())
    {
        std::cout << "Port already started.\n";
        return false;
    }
    if (!connected)
    {
        std::cout << "Cannot start port without a connection.\n";
        return false;
    }
    if (!initialized)
    {
        std::cout << "Must initialize port before starting it.\n";
        return false;
    }

    {
        std::lock_guard<std::mutex> g(running_state_m);
        running_state = PortState::Running;
    }

    clear_buffer();

#ifdef OS_CYGWIN
    // /*
    //  * Create thread.
    //  */
    // thread_term = CreateEvent(NULL,   // No security attributes
    //                           false,  // No manual reset
    //                           false,  // starts nonsignaled
    //                           NULL);  // no name
    // thread_started = CreateEvent(NULL,   // No security attributes
    //                              false,  // No manual reset
    //                              false,  // starts nonsignaled
    //                              NULL);  // no name
    //
    // thread_handle = std::thread(&ComPort::async_receive, (void*)this);
    // thread_handle.detach();
    //
    // DWORD wait_rv = WaitForSingleObject(thread_started, INFINITE);
    //
    // assert(wait_rv == WAIT_OBJECT_0);
    //
    // CloseHandle(thread_started);
    // invalidate_handle(thread_started);
#elif OS_LINUX
#endif

    return true;
}

void ComPort::stop()
{
#ifdef OS_CYGWIN
    // {
    //     std::lock_guard<std::mutex> g(running_state_m);
    //     running_state = PortState::Stopped;
    // }
    // SetEvent(thread_term);
#elif OS_LINUX
#endif
}

#include <stdio.h>

std::vector<unsigned int> ComPort::find_ports()
{
    std::vector<unsigned int> found_ports{};

#ifdef OS_CYGWIN
    std::string prefix = "\\\\.\\COM";

    for (std::size_t i = COM_BEG; i < COM_END; i++)
    {
        std::string com_port_str = prefix + std::to_string(i);

        std::cout << "Checking COM" << i << "...\n";
        handle = CreateFile(com_port_str.c_str(),  // filename
                            GENERIC_READ,          // access method
                            0,                     // cannot share
                            NULL,                  // no security attributes
                            OPEN_EXISTING,         // file action, value for serial ports
                            0,                     // FILE_FLAG_OVERLAPPED TODO change
                            NULL);                 // ignored

        if (is_valid())
        {
            std::cout << "COM" << i << " available\n";
            found_ports.push_back(i);
        }
        CloseHandle(handle);
    }
#elif OS_LINUX
    libusb_device** libusb_devs;
    std::size_t dev_count;

    dev_count = libusb_get_device_list(NULL, &libusb_devs);
    if (dev_count < 0)
    {
        return std::vector<unsigned int>{};
    }

    libusb_device* libusb_dev;
    libusb_device_handle* libusb_handle;
    int i = 0;
    int j = 0;
    std::uint8_t path[8];
    std::array<unsigned char, 128> summary;

    while ((libusb_dev = libusb_devs[i++]) != NULL)
    {
        struct libusb_device_descriptor desc;

        int r = libusb_get_device_descriptor(libusb_dev, &desc);
        if (r < 0)
            std::cout << "couldn't get device descriptor\n";

        printf("%04x:%04x (bus %d, device %d) ",
               desc.idVendor,
               desc.idProduct,
               libusb_get_bus_number(libusb_dev),
               libusb_get_device_address(libusb_dev));

        int status = libusb_open(libusb_dev, &libusb_handle);
        if (status < 0)
        {
            std::cout << "libusb_open failed with " << status << std::endl;
        }
        else
        {
            summary.fill('\0');
            r = libusb_get_string_descriptor_ascii(libusb_handle, desc.iManufacturer, summary.data(), summary.size());
            std::string m_str;
            m_str.assign(std::begin(summary), std::find(std::begin(summary), std::end(summary), static_cast<unsigned char>('\0')));
            std::cout << m_str << " | ";
            summary.fill('\0');
            r = libusb_get_string_descriptor_ascii(libusb_handle, desc.iProduct, summary.data(), summary.size());
            std::string p_str;
            p_str.assign(std::begin(summary), std::find(std::begin(summary), std::end(summary), static_cast<unsigned char>('\0')));
            std::cout << p_str;

            libusb_close(libusb_handle);
            printf("\n");
        }
    }
    printf("\n");

    libusb_free_device_list(libusb_devs, 1);
#endif

    available_ports = found_ports;
    return found_ports;
}

bool ComPort::connect(unsigned int port)
{
    if (connected)
    {
        std::cout << "Port is already connected\n";
        return false;
    }
    std::string prefix = "\\\\.\\COM";
    std::string com_port_str = prefix + std::to_string(port);

#ifdef OS_CYGWIN
    // handle = CreateFile(com_port_str.c_str(),  // filename
    //                     GENERIC_READ,          // access method
    //                     0,                     // cannot share
    //                     NULL,                  // no security attributes
    //                     OPEN_EXISTING,         // file action, value for serial ports
    //                     0,                     // FILE_FLAG_OVERLAPPED TODO change
    //                     NULL);                 // ignored
#elif OS_LINUX
#endif

    if (is_valid())
    {
        std::cout << "Successfully opened COM" << port << '\n';
        connected = true;
        connected_port = port;
        return true;
    }
    return false;
}

bool ComPort::auto_connect()
{
    std::string prefix = "\\\\.\\COM";

    for (std::size_t i = COM_BEG; i < COM_END; i++)
    {
        std::string com_port_str = prefix + std::to_string(i);

        std::cout << "Attempting to open COM" << i << "...\n";
#ifdef OS_CYGWIN
        // handle = CreateFile(com_port_str.c_str(),  // filename
        //                     GENERIC_READ,          // access method
        //                     0,                     // cannot share
        //                     NULL,                  // no security attributes
        //                     OPEN_EXISTING,         // file action, value for serial ports
        //                     0,                     // FILE_FLAG_OVERLAPPED TODO change
        //                     NULL);                 // ignored
#elif OS_LINUX
#endif

        if (is_valid())
        {
            std::cout << "Successfully opened COM" << i << '\n';
            connected = true;
            connected_port = i;
            return true;
        }
    }
    std::cout << "Could not find an available COM port. Aborting.\n";
    return false;
}

void ComPort::disconnect()
{

}

bool ComPort::is_valid() const
{
#ifdef OS_CYGWIN
    return !(handle == INVALID_HANDLE_VALUE);
#elif OS_LINUX
    return true;  // TODO fix
#endif
}

std::shared_ptr<std::string> ComPort::get_latest_packet()
{
    char tmp{};
    std::shared_ptr<char> result{};

    // If necessary, start building new packet.
    result = buffer->try_pop();
    if (build_new_packet)
    {
        while (result)
        {
            if (*result == packet_start_symbol)
                break;
            result = buffer->try_pop();
        }
    }

    // Extract the rest of the packet.
    while (result)
    {
        if (*result == packet_stop_symbol)
            break;
        latest_packet += *result;
        result = buffer->try_pop();
    }

    // Complete packet not built yet.
    if (latest_packet.size() < packet_len)
    {
        build_new_packet = false;
        return nullptr;
    }

    // Finished building packet.
    if ((latest_packet.size() == packet_len) &&
        (latest_packet[0] == packet_start_symbol))
    {
        std::string rv = latest_packet;
        latest_packet.clear();
        build_new_packet = true;
        return std::make_shared<std::string>(rv);
    }

    // Packet is complete but corrupted, restart.
    latest_packet.clear();
    build_new_packet = true;
    return nullptr;
}

#ifdef OS_CYGWIN
void ComPort::invalidate_handle(HANDLE& handle)
{
    handle = INVALID_HANDLE_VALUE;
}
#elif OS_LINUX
void ComPort::invalidate_handle()
{
    // TODO: fill in.
}
#endif

unsigned ComPort::async_receive(void* params)
{
    ComPort* com_ptr = static_cast<ComPort*>(params);
    bool keep_processing = true;
    // DWORD event_mask = 0;

    // OVERLAPPED ov;
    // memset(&ov, 0, sizeof(ov));
    // ov.hEvent = CreateEvent(NULL,   // event can't be inherited
    //                         true,   // manual reset
    //                         false,  // initially nonsignaled
    //                         NULL);  // no name
    // HANDLE handles[2];
    // handles[0] = com_ptr->thread_term;
    //
    // DWORD wait_rv;
    // SetEvent(com_ptr->thread_started);
    //
    // while (keep_processing)
    // {
    //     if (!WaitCommEvent(com_ptr->handle, &event_mask, &ov))
    //     {
    //         std::cout << "GetLastError() = " << GetLastError() << '\n';
    //         assert(GetLastError() == ERROR_IO_PENDING);
    //     }
    //
    //     handles[1] = ov.hEvent;
    //     wait_rv = WaitForMultipleObjects(2,          // # object handles
    //                                      handles,    // array of object handles
    //                                      FALSE,      // return when any object is signaled
    //                                      INFINITE);  // no timeout
    //
    //     switch (wait_rv)
    //     {
    //     case WAIT_OBJECT_0:
    //         {
    //             return 0;
    //         }
    //     case WAIT_OBJECT_0 + 1:
    //         {
    //             try
    //             {
    //                 BOOL read_rv = false;
    //                 DWORD bytes_read = 0;
    //
    //                 std::string tmp_str{};
    //
    //                 OVERLAPPED ov_read;
    //                 memset(&ov_read, 0, sizeof(ov_read));
    //                 ov_read.hEvent = CreateEvent(NULL,   // no security attributes
    //                                              true,   // manual reset
    //                                              false,  // initially nonsignaled
    //                                              NULL);  // no name
    //
    //                 do
    //                 {
    //                     ResetEvent(ov_read.hEvent);
    //
    //                     char tmp_buf[1];
    //                     int i_size = sizeof(tmp_buf);
    //                     memset(tmp_buf, 0, sizeof(tmp_buf));
    //
    //                     read_rv = ReadFile(com_ptr->handle,  // device handle
    //                                        tmp_buf,          // receiving buffer
    //                                        sizeof(tmp_buf),  // max # bytes to read
    //                                        &bytes_read,      // # bytes read
    //                                        &ov_read);        // OVERLAPPED struct ptr
    //                     if (!read_rv)
    //                     {
    //                         keep_processing = false;
    //                         break;
    //                     }
    //                     if (bytes_read > 0)
    //                     {
    //                         com_ptr->buffer->force_push(tmp_buf[0]);
    //                     }
    //                 } while (bytes_read > 0);
    //                 CloseHandle(ov_read.hEvent);
    //             }
    //             catch(...)
    //             {
    //                 std::cout << "Error: Serial reading thread failed!";
    //                 assert(0);
    //             }
    //             ResetEvent(ov.hEvent);
    //             break;
    //         }
    //     }
    // }
    return 0;
}
