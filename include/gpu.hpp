#ifndef U_CONCURRENT_CL_HPP
#define U_CONCURRENT_CL_HPP

#include <algorithm>
#include <string> 

#include <boost/compute.hpp>
#include <boost/compute/system.hpp>

namespace concurrent {
namespace cl {

class Task {
public:
    typedef std::shared_ptr<Task> Ptr;

};

class Device {
public:
    typedef std::shared_ptr<Device> Ptr;

    Device() {
        _device = boost::compute::system::default_device();
        _ctx = boost::compute::system::default_context();
        _queue = boost::compute::system::default_queue();
    }

private:
    boost::compute::device _device;
    boost::compute::context _ctx;
    boost::compute::command_queue _queue;
};

class Host {
public:
    typedef std::shared_ptr<Host> Ptr;

    enum Platform {
        ANY = 0,
        AMD = 1 << 0,
        INTEL = 1 << 1,
        NVIDIA = 1 << 2
    };

    static Host::Ptr WithPlatform(Platform p) {
        Host::Ptr host(nullptr);
        
        auto platforms = boost::compute::system::platforms();
        for (const auto& platform : platforms) {
            auto name = platform.name();
            std::transform(name.begin(), name.end(), name.begin(), ::tolower);

            auto vendor = platform.vendor();
            std::transform(vendor.begin(), vendor.end(), vendor.begin(), ::tolower);

            switch (p) {
                case AMD: {
                    if (name.find("amd") != std::string::npos || vendor.find("amd") != std::string::npos) {
                        host.reset(new Host(platform));
                        return host;
                    }
                    break;
                }
                case INTEL: {
                    if (name.find("intel") != std::string::npos || vendor.find("intel") != std::string::npos) {
                        host.reset(new Host(platform));
                        return host;
                    }
                    break;
                }
                case NVIDIA: {
                    if (name.find("nvidia") != std::string::npos || vendor.find("nvidia") != std::string::npos) {
                        host.reset(new Host(platform));
                        return host;
                    }
                    break;
                }
                case ANY: {
                    host.reset(new Host(platform));
                    return host;
                }
            }
        }
        return host;
    }

    const boost::compute::platform& platform() const { return _platform; }

private:
    Host(boost::compute::platform p) : _platform(p) {}

    boost::compute::platform _platform;
};

}}

#endif