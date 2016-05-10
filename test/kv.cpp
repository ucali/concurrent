#include "catch.hpp"

#include "kv.hpp"
#include "queue.hpp"
#include "pool.hpp"

#include <assert.h>

#include <iostream>

TEST_CASE("TestMap") {
    concurrent::SyncMap<int, int> map;
    map.Insert(1, 1);
    REQUIRE(map.Get(1) == 1);

    REQUIRE(map.Remove(1));
    REQUIRE_FALSE(map.Remove(1));

    REQUIRE_FALSE(map.Find(1));
    REQUIRE_THROWS(map.Get(1));

    map.Clear();
}

TEST_CASE("TestHashMap") {
    concurrent::SyncHashMap<int, int> hash;
    hash.Insert(1, 1);

    REQUIRE(hash.Get(1) == 1);

    REQUIRE(hash.Remove(1));
    REQUIRE_FALSE(hash.Remove(1));

    REQUIRE_FALSE(hash.Find(1));
    REQUIRE_THROWS(hash.Get(1));
    hash.Clear();
}

TEST_CASE("TestMapCallback", "DefaultPool") {
	std::cout << "TestMapCallback -> " << std::endl;

    concurrent::SyncQueue<int>::Ptr in(new concurrent::SyncQueue<int>());
    concurrent::SyncMap<int, int>::Ptr out(new concurrent::SyncMap<int, int>());

    auto& pool = concurrent::SystemTaskPool<>();

    pool.Send([in, out] {
		while (in->CanReceive()) {
			auto val = in->Pop();
			out->Insert(val, val);
		}
    }, [out] {
		assert(out->Size() == 1000);
		assert(out->Get(0) == 0);
		assert(out->Get(999) == 999);

		assert(out->Remove(999));
		assert(out->Size() == 999);

        out->Clear();
    });

    for (int i = 0; i < 1000; i++) {
        in->Push(i);
    }
    in->Close();

	std::cout << "<- TestMapCallback" << std::endl;
}

TEST_CASE("TestMapPipeline") {
	std::cout << "TestMapPipeline -> " << std::endl;

    concurrent::SyncQueue<int>::Ptr in(new concurrent::SyncQueue<int>());
    concurrent::SyncMap<int, int>::Ptr out(new concurrent::SyncMap<int, int>());

    concurrent::WaitGroup::Ptr wg(new concurrent::WaitGroup(4));

    auto& pool = concurrent::SystemTaskPool<>();

    pool.Send([in, out, wg] {
		try {
			while (in->CanReceive()) {
				auto val = in->Pop();
				out->Insert(val, val);
			}
		} catch (const std::exception& e) { 
			std::cerr << e.what() << std::endl; 
		}

        wg->Finish();
    }, wg->Size());

    pool.Send([in, out, wg] {
        wg->Wait();
    });

    for (int i = 0; i < 1000; i++) {
        in->Push(i);
    }
    in->Close();

	out->ForEach([](const std::pair<int, int>&) {
		
	});

	out->Clear();

	std::cout << "<- TestMapPipeline" << std::endl;
}
