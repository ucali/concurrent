#include "catch.hpp"

#include "pool.hpp"

#include <iostream>
#include <assert.h>

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
			assert(a == 1);
			assert(b == "test");
            return a*2.0f;
        }, [] (auto a) {
			assert(a == 2.0f);
        }, 1, std::string("test")
    );
}

TEST_CASE("TestPoolSpawnSimple") {
    concurrent::Pool<> simple(2);

    CHECK(simple.Size() == 2);

    simple.Spawn<int, std::string>(
        [] (int a, std::string& b){
			assert(a == 1);
			assert(b == "test");
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
    concurrent::SystemTaskPool<>().Spawn(
        [] (){
            while (concurrent::SystemTaskPool<>().IsRunning()) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
            std::cout << "Shutting down default pool.." << std::endl;
        }
    );

    REQUIRE(concurrent::SystemTaskPool<>().Size() == 5);
}


TEST_CASE("TestPoolProcessing") {
    using namespace concurrent;

    concurrent::SyncQueue<int>::Ptr list(new concurrent::SyncQueue<int>());
    concurrent::SyncMap<int, double>::Ptr map(new concurrent::SyncMap<int, double>());

    concurrent::SystemTaskPool<>().Map<int, int, double>(list, map, [] (concurrent::SyncQueue<int>::Ptr, concurrent::SyncMap<int, double>::Ptr) {
        std::cout << "Map" << std::endl;
    })->Collect<int, double, int>(map, list, [] ( concurrent::SyncMap<int, double>::Ptr, concurrent::SyncQueue<int>::Ptr) {
        std::cout << "Collect" << std::endl;
    })->Collect<int, int>(list, list, [] ( concurrent::SyncQueue<int>::Ptr, concurrent::SyncQueue<int>::Ptr) {
        std::cout << "Collect" << std::endl;
    })->Filter<int>(list, list, [] ( concurrent::SyncQueue<int>::Ptr, concurrent::SyncQueue<int>::Ptr) {
        std::cout << "Filter" << std::endl;
    });


    _StreamItem<typename SyncQueue<int>, typename SyncQueue<int>> item;
    item.MapStream<int, int, int>([] (int i) {
        std::cout << i << std::endl;
        return std::move(std::pair<int, int>(i, i));
    });

    item.Input()->Push(1);
    item.Input()->Push(2);

}
