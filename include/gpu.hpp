#ifndef U_CONCURRENT_CL_HPP
#define U_CONCURRENT_CL_HPP

#include <algorithm>
#include <string> 

#include <boost/compute.hpp>
#include <boost/compute/system.hpp>

#include "pool.hpp"

namespace concurrent {
namespace cl {

typedef std::function<void (boost::compute::context&, boost::compute::command_queue&)> TaskFunc;

class Task {
public:
    typedef std::shared_ptr<Task> Ptr;
    virtual ~Task() { }

    virtual void Run(boost::compute::context&, boost::compute::command_queue&) = 0;
};

class FuncEnvelope : public Task {
public:
    FuncEnvelope(const TaskFunc& f) : _func(f) {}

    virtual void Run(boost::compute::context& c, boost::compute::command_queue& q) override {
        _func(c, q);
    }

private:
    TaskFunc _func;
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

    void Compute(Task::Ptr task) {
        task->Run(_context, _queue);
    }

    void Compute(const TaskFunc& func) {
        func(_context, _queue);
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

    size_t CountGPU() const { return _gpus.size(); }
    size_t CountCPU() const { return _cpus.size(); }
    size_t Count() const { return CountGPU() + CountCPU(); }

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

class ComputeBridge {
public:
    ComputeBridge(Host::Platform platform) : _host(cl::Host::WithPlatform(platform)) {}
    ~ComputeBridge() { Close(); }

    void Close() { 
        _cpuQ.Close();
        _gpuQ.Close();
        
        _pool->Close();
    }

    void ComputeOnGPU(Task::Ptr task) {
        _gpuQ.Push(task);
    }

    void ComputeOnGPU(const TaskFunc& func) {
        _gpuQ.Push(Task::Ptr(new FuncEnvelope(func)));
    }

    void ComputeOnCPU(Task::Ptr task) {
       _cpuQ.Push(task);
    }

    void ComputeOnCPU(const TaskFunc& func) {
        _cpuQ.Push(Task::Ptr(new FuncEnvelope(func)));
    }

protected:
    void _loop(Channel::Ptr chan, SyncQueue<Task::Ptr>& queue) {
        while (queue.CanReceive()) {
            auto task = queue.Pop();
            if (task != nullptr) {
                chan->Compute(task);
            }
        }
    }

    SyncQueue<Task::Ptr>& cpuQ() { return _cpuQ; }
    SyncQueue<Task::Ptr>& gpuQ() { return _gpuQ; }
    Host::Ptr host() { return _host; }
    
	Pool<>::Ptr _pool;

private:
    Host::Ptr _host;
    
    SyncQueue<Task::Ptr> _cpuQ;
    SyncQueue<Task::Ptr> _gpuQ;
};

class SharedComputeBridge : public ComputeBridge {
public:
    SharedComputeBridge(Host::Platform platform) : ComputeBridge(platform) {
        Context::Ptr context = host()->NewFullContext();

        _pool.reset(new Pool<>(2));
        _pool->Send([this, context] {
            auto chan = context->CPU(0);
            _loop(chan, cpuQ());
        });
        _pool->Send([this, context] {
            auto chan = context->GPU(0);
            _loop(chan, gpuQ());
        });
    }
};

class IndipendentComputeBridge : public ComputeBridge {
public:
    IndipendentComputeBridge(Host::Platform platform) : ComputeBridge(platform) {
        _pool.reset(new Pool<>(2));
        Context::Ptr cpu = host()->NewCPU();
        _pool->Send([this, cpu] {
            auto chan = cpu->CPU(0);
            _loop(chan, cpuQ());
        });

        Context::Ptr gpu = host()->NewGPU();
        _pool->Send([this, gpu] {
            auto chan = gpu->GPU(0);
            _loop(chan, gpuQ());
        });
    }
};

}}

#endif