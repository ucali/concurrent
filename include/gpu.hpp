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

    virtual void Run(boost::compute::command_queue&) = 0;

};

class Channel {
public:
    typedef std::shared_ptr<Channel> Ptr;

     Channel(boost::compute::device device) {
        _device = device;
        _context = boost::compute::context(device);
        _queue = boost::compute::command_queue(_context, _device);
    }

     Channel(boost::compute::device device, boost::compute::context context) {
        _device = device;
        _context = context;
        _queue = boost::compute::command_queue(_context, _device);
    }

    bool OnGPU() const {
        return _device.type() == CL_DEVICE_TYPE_GPU;
    }

    bool OnCPU() const {
        return _device.type() == CL_DEVICE_TYPE_CPU;
    }

    void Push(Task::Ptr task) {

    }

    const boost::compute::device& device() const { return _device; }
    const boost::compute::context& context() const { return _context; }
    const boost::compute::command_queue& queue() const { return _queue; }

private:
    boost::compute::device _device;
    boost::compute::context _context;
    boost::compute::command_queue _queue;
};

class Context {
public:
    typedef std::shared_ptr<Context> Ptr;

    void Add(Channel::Ptr ch) {
         if (ch->OnCPU()) {
              _cpus.push_back(ch);
         } else if (ch->OnGPU()) {
             _gpus.push_back(ch);
         } else {
            throw new std::runtime_error("Unknown device");
         }
    }

    const Channel::Ptr GPU(short i) const { return _gpus.at(i); }
    const Channel::Ptr CPU(short i) const { return _cpus.at(i); }

private:
    std::vector<Channel::Ptr> _gpus;
    std::vector<Channel::Ptr> _cpus;
};

class Host {
public:
    typedef std::shared_ptr<Host> Ptr;

    enum Platform {
        NONE = 0,
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
                        host.reset(new Host(p, platform));
                        return host;
                    }
                    break;
                }
                case INTEL: {
                    if (name.find("intel") != std::string::npos || vendor.find("intel") != std::string::npos) {
                        host.reset(new Host(p, platform));
                        return host;
                    }
                    break;
                }
                case NVIDIA: {
                    if (name.find("nvidia") != std::string::npos || vendor.find("nvidia") != std::string::npos) {
                        host.reset(new Host(p, platform));
                        return host;
                    }
                    break;
                }
            }
        }
        return host;
    }

    Context::Ptr NewFullContext() {
        Context::Ptr context(new Context);

        auto all = _platform.devices();
        boost::compute::context clCtx(all);

        for (auto d : all) {
            Channel::Ptr dev(new Channel(d, clCtx));
            context->Add(dev);
        }
        return context;
    }

    Context::Ptr NewGPU() {
        auto devices = _platform.devices(CL_DEVICE_TYPE_GPU);
        if (devices.size() == 0) {
            return nullptr;
        }

        Context::Ptr context(new Context);
        auto clDev = devices.at(0);
        boost::compute::context clCtx(clDev);

        Channel::Ptr dev(new Channel(clDev, clCtx));
        context->Add(dev);
        return context;
    }

     Context::Ptr NewCPU() {
        auto devices = _platform.devices(CL_DEVICE_TYPE_CPU);
        if (devices.size() == 0) {
            return nullptr;
        }

        Context::Ptr context(new Context);
        auto clDev = devices.at(0);
        boost::compute::context clCtx(clDev);

        Channel::Ptr dev(new Channel(clDev, clCtx));
        context->Add(dev);
        return context;
    }

    const boost::compute::platform& platform() const { return _platform; }
    const Platform& platformId() const { return _platformId; }

private:
    Host(Platform id, boost::compute::platform p) : _platformId(id), _platform(p) {}

    const boost::compute::platform _platform;
    const Platform _platformId;
};




}}

#endif