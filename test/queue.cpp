#include "catch.hpp"

#include "queue.hpp"
#include "pool.hpp"

#include <iostream>

class EmptyItem {
public:
    EmptyItem() {}
};

TEST_CASE("TestQueueNullSimple") {
	std::cout << "TestQueueNullSimple -> " << std::endl;

    concurrent::SyncQueue<EmptyItem*> sync;
    sync.Push(nullptr);
    sync.Push(nullptr);

    REQUIRE(sync.Pop() == nullptr);
    REQUIRE(sync.Pop() == nullptr);

	std::cout << "<- TestQueueNullSimple" << std::endl;
}

TEST_CASE("TestQueueTimeout") {
	std::cout << "TestQueueTimeout -> " << std::endl;

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

	std::cout << "<- TestQueueTimeout" << std::endl;
}

TEST_CASE("TestQueueOrder") {
	std::cout << "TestQueueOrder -> " << std::endl;

    concurrent::SyncQueue<int> sync;
    sync.Push(1);
    sync.Push(2);

    REQUIRE(sync.Pop() == 1);
    REQUIRE(sync.Pop() == 2);

	std::cout << "<- TestQueueOrder" << std::endl;
}

TEST_CASE("TestQueuePipeline", "DefaultPool") {
	std::cout << "TestQueuePipeline -> " << std::endl;


    concurrent::SyncQueue<int> in;
    concurrent::SyncQueue<int> out;

    auto& pool = concurrent::GetPool<>();

    pool->Send([&in] {

		std::cout << "TID: " << std::this_thread::get_id() << std::endl;

        in.Push(1);
        in.Push(2);
        in.Push(3);

        in.Close();

		std::cout << "TID: " << std::this_thread::get_id() << std::endl;
    });

    pool->Send([&in, &out] {
		std::cout << "TID: " << std::this_thread::get_id() << std::endl;
		try {
			while (in.CanReceive()) {
				out.Push(in.Pop() + 1);
			}
		}
		catch (...) {}
        out.Close();
    });


    REQUIRE_NOTHROW(out.Pop());
    REQUIRE_NOTHROW(out.Pop());
    REQUIRE_NOTHROW(out.Pop());
    REQUIRE_THROWS(out.Pop());

    REQUIRE(out.Size() == 0);

    std::cout << "MAIN TID: " << std::this_thread::get_id() << std::endl;


	std::cout << "<- TestQueuePipeline" << std::endl;
}
