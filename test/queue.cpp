#include "catch.hpp"

#include "queue.hpp"
#include "pool.hpp"

#include <iostream>

class EmptyItem {
public:
    EmptyItem() {}
};

TEST_CASE("TestQueueNullSimple") {
    concurrent::SyncQueue<EmptyItem*> sync;
    sync.Push(nullptr);
    sync.Push(nullptr);

    REQUIRE(sync.Pop() == nullptr);
    REQUIRE(sync.Pop() == nullptr);
}

TEST_CASE("TestQueueTimeout") {
    concurrent::SyncQueue<std::string> sync(2);
    REQUIRE(sync.Pop(100) == std::string());

    sync.Push(std::move(std::string("ok")), 100);
    REQUIRE(sync.Pop(100) == "ok");
    REQUIRE(sync.Pop(100) == std::string());

    REQUIRE(sync.Push(std::move(std::string("ok")), 100));
    REQUIRE(sync.Push(std::move(std::string("ok")), 100));

    REQUIRE_FALSE(sync.Push(std::move(std::string("ko")), 100));
    INFO(sync.Size());

    std::string value;
    concurrent::SyncQueue<std::string*> sync_ptr;
    REQUIRE(sync_ptr.Pop(100) == nullptr);

    sync_ptr.Push(&value);
    REQUIRE(sync_ptr.Pop(100) == &value);
    REQUIRE(sync_ptr.Pop(100) == nullptr);
}

TEST_CASE("TestQueueOrder") {
    concurrent::SyncQueue<int> sync;
    sync.Push(1);
    sync.Push(2);

    REQUIRE(sync.Pop() == 1);
    REQUIRE(sync.Pop() == 2);
}

TEST_CASE("TestQueuePipeline") {

    concurrent::SyncQueue<int> in;
    concurrent::SyncQueue<int> out;

    auto& pool = concurrent::SystemTaskPool<>();

    concurrent::SystemTaskPool<>().Spawn(
        [] (){
            while (concurrent::SystemTaskPool<>().IsRunning()) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
            std::cout << "Shutting down default pool.." << std::endl;
        }
    );

    pool.Send([&in] {
        std::cout << "TID: " << std::this_thread::get_id() << std::endl;

        in.Push(1);
        in.Push(2);
        in.Push(3);

        in.Close();

        std::this_thread::sleep_for(std::chrono::seconds(1));
    });

    pool.Send([&in, &out] {
        std::cout << "TID: " << std::this_thread::get_id() << std::endl;

        while (in.CanReceive()) {
            out.Push(in.Pop() + 1);
        }
        out.Close();

        std::this_thread::sleep_for(std::chrono::seconds(1));
    });


    REQUIRE_NOTHROW(out.Pop());
    REQUIRE_NOTHROW(out.Pop());
    REQUIRE_NOTHROW(out.Pop());
    REQUIRE_THROWS(out.Pop());

    REQUIRE(out.Size() == 0);

    std::cout << "MAIN TID: " << std::this_thread::get_id() << std::endl;

}
