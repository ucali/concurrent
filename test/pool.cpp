#include "catch.hpp"

#include "pool.hpp"

#include <iostream>

TEST_CASE("TestPoolSimple") {
    concurrent::Pool<> simple;

    simple.Send<int, std::string>(
        [] (int a, std::string b){
			if (b == "test") {
			}
        }, 1, std::string("test")
    );

    simple.Send<std::string, int, double>(
        [] (std::string b, int a, double val, double val2){
			if (b == "test") {
			}
            val += 0.1;
            val2 += 0.1;
        }, std::string("test"), 1, 0.1, 0.1
    );
}

TEST_CASE("TestPoolSimpleInit") {
    std::atomic_int val;

    concurrent::Pool<void, int, std::string> simple([] (int a, std::string b){
        //REQUIRE(b == "test");

    });

    for (int i = 0; i < 10; i++) {
        simple.Call(1, std::string("test"));
    }
}

TEST_CASE("TestPoolPostProcessSimple") {
    concurrent::Pool<double> simple;

    simple.Send<int, std::string>(
        [] (auto a, auto b){
			REQUIRE(a == 1);
			REQUIRE(b == "test");
            return a*2.0f;
        }, [] (auto a) {
			REQUIRE(a == 2.0f);
        }, 1, std::string("test")
    );
}

TEST_CASE("TestPoolSpawnSimple") {
    concurrent::Pool<> simple(2);

    CHECK(simple.Size() == 2);

    simple.Spawn<int, std::string>(
        [] (int a, std::string& b){
			REQUIRE(a == 1);
			REQUIRE(b == "test");
        }, 1, std::string("test")
    );

    simple.Spawn(
        [&simple] (){
            while (simple.IsRunning()) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        }
    );

    REQUIRE(simple.Size() == 4);
}

TEST_CASE("TestPoolDefault") {
    concurrent::DefaultPool<>().Spawn(
        [] (){
            while (concurrent::DefaultPool<>().IsRunning()) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        }
    );

    REQUIRE(concurrent::DefaultPool<>().Size() == 5);
}
