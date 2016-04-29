#include "catch.hpp"

#include "kv.hpp"

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
