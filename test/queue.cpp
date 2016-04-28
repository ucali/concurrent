#include "catch.hpp"

#include "queue.hpp"

class EmptyItem {
public:
    EmptyItem() {}
};

TEST_CASE("TestQueueSimple") {
    concurrent::SyncQueue<EmptyItem*> sync;
    sync.Push(nullptr);
    sync.Push(nullptr);

    CHECK(sync.Pop() == nullptr);
    CHECK(sync.Pop() == nullptr);
}

TEST_CASE("TestQueueTimeout") {
    concurrent::SyncQueue<std::string> sync(2);
    CHECK(sync.Pop(100) == std::string());

    sync.Push(std::move(std::string("ok")), 100);
    CHECK(sync.Pop(100) == "ok");
    CHECK(sync.Pop(100) == std::string());

    CHECK(sync.Push(std::move(std::string("ok")), 100));
    CHECK(sync.Push(std::move(std::string("ok")), 100));

    CHECK_FALSE(sync.Push(std::move(std::string("ko")), 100));
    INFO(sync.Size());

    std::string value;
    concurrent::SyncQueue<std::string*> sync_ptr;
    CHECK(sync_ptr.Pop(100) == nullptr);

    sync_ptr.Push(&value);
    CHECK(sync_ptr.Pop(100) == &value);
    CHECK(sync_ptr.Pop(100) == nullptr);
}
