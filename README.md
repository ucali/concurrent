# Simple concurrent processing library

[![Build Status](https://travis-ci.org/ucali/concurrent.svg?branch=master)](https://travis-ci.org/ucali/concurrent)
[![Build status](https://ci.appveyor.com/api/projects/status/6hj2ar4d2goq54rm/branch/master?svg=true)](https://ci.appveyor.com/project/ucali/concurrent/branch/master)


Header-only library to simplify long running multistep processing and concurrent computation.
Optional boost dependency for fibers and opencl 1.1+.

Tested with msvc14, gcc5, clang3.7.

TODO: fix boost build with clang.

## Examples:

GPU:

```c++

cl::SharedComputeBridge bridge(cl::Host::AMD);
bridge.ComputeOnCPU([] (auto& context, auto& queue) {});
bridge.ComputeOnGPU([] (auto& context, auto& queue) {});
bridge.Close();

```

Fibers:

```c++

concurrent::FiberScheduler fibers;

int i = 0;
fibers.Run([&i] {
    while (i < 1000) {
        i++;
        concurrent::yield();
    }
});

int j = 0;
fibers.Run([&j] {
    while (j < 1000) {
        j++;
        concurrent::yield();
    }
});

concurrent::Channel<int> chan;

i = 0;
fibers.Run([&i, &chan] {
    concurrent::ChannelStatus status = concurrent::ChannelStatus::empty;
    while (status != concurrent::ChannelStatus::closed) {
        status = chan.try_pop(i);
    }
});

j = 0;
fibers.Run([&j, &chan]  {
    while (j < 100000) {
        j++;
        chan.push(j);
    }
    chan.close();
});


fibers.Close();

```

Processing samples:

```c++
...

auto result = Streamer<Test>(input.begin(), input.end()).Filter([](Test k) {
    return k.status == true;
}, 2)->KV<std::map<int64_t, Test>>([](Test t) {
	return std::make_pair(t.id, t);
}, 2);

auto count = result->Reduce<size_t>([] (auto t, size_t& s) {
    return s + 1;
});
```

```c++
...
Streamer<int> item;
auto result = item.KV<std::multimap<int, int>>([] (int i) {
    return std::make_pair(i, i);
})->Transform<int>([](auto k) {
    return k.first + k.second;
});

auto input = item.Input();
for (int i = 0; i < 1000; i++) {
    input->Push(i);
}
```

```c++

using namespace concurrent;
...

Pool<>::Ptr pool(new concurrent::Pool<>);
	
Streamer<int>(
     input.begin(), 
     input.end(), 
     pool
).KV<std::multimap<int, int>>([] (int t) {
	return std::move(std::make_pair(t, t));
})->PartitionMT<std::vector<int>, int>([] (const auto& k, auto vec) {
	assert(vec->size() == 2);
	return vec->size();
})->ForEach([&v2] (auto v) {
	v2 += v;
});
```


Task pool samples:

```c++
concurrent::Pool<double> pool;

std::function<double (int, std::string)> fun = [] (int a, std::string b) {
    assert(b == "test");
    return a*2.0f;
};

std::function<void (double)> cb = [] (double a) {
    assert(a == 2.0f);
};

for (int i = 0; i < 10; i++) {
    pool.Send<int, std::string>(fun, cb, 1, std::string("test"));
}
```

```c++
concurrent::Pool<void, int, std::string> pool([] (int a, std::string b){
    assert(b == "test");
});

for (int i = 0; i < 10; i++) {
    pool.Call(1, std::string("test"));
}
```
