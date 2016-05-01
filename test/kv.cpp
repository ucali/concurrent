#include "catch.hpp"

#include "kv.hpp"
#include "queue.hpp"
#include "pool.hpp"

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

TEST_CASE("TestMapPipeline") {

    concurrent::SyncQueue<int>::Ptr in(new concurrent::SyncQueue<int>());
    concurrent::SyncMap<int, int>::Ptr out(new concurrent::SyncMap<int, int>());

    auto& pool = concurrent::DefaultPool<>();

    pool.Send([in, out] {
		while (in->CanReceive()) {
			auto val = in->Pop();
			out->Insert(val, val);
		}
    }, [out] {
		REQUIRE(out->Size() == 2);
		REQUIRE(out->Get(1) == 1);
		REQUIRE(out->Get(2) == 2);

		out->Clear();
		REQUIRE_THROWS(out->Get(3));

		out->Get(3);
    });

    in->Push(1);
    in->Push(2);
    in->Close();

}

