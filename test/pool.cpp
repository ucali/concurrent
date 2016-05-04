#include "catch.hpp"

#include "pool.hpp"

#include <iostream>
#include <assert.h>

TEST_CASE("TestPoolSimple") {
    concurrent::Pool<> simple;

	std::function<void (int, std::string)> fun = [](int a, std::string b) {
		if (b == "test") {
		}
		return;
	};

    simple.Send<int, std::string>(fun, 1, std::string("test"));


	std::function<void (std::string, int, double, double)> fn2 = [](std::string b, int a, double val, double val2) {
		if (b == "test") {
		}
		val += 0.1;
		val2 += 0.1;
	};
    simple.Send<std::string, int, double, double>(fn2, std::string("test"), 1, 0.1, 0.1);
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

	std::function<double (int, std::string)> fun = [](int a, std::string b) {
		assert(a == 1);
		assert(b == "test");
		return a*2.0f;
	};

	std::function<void (double)> cb = [](double a) {
		assert(a == 2.0f);
	};

    simple.Send<int, std::string>(fun, cb, 1, std::string("test")
    );
}

TEST_CASE("TestPoolSpawnSimple") {
    concurrent::Pool<> simple(2);

    CHECK(simple.Size() == 2);

    std::function<void (int, std::string)> fun = [] (int a, std::string b){
        assert(a == 1);
        assert(b == "test");
    };
    simple.Spawn<int, std::string>(fun, 1, std::string("test"));

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
}


TEST_CASE("TestPoolProcessing") {
    using namespace concurrent;

	Streamer<int> item;
    item.Map<int, int, int>([] (int i) {
        return std::move(std::pair<int, int>(i, i));
    })->Reduce<int, int, int>([](int k, int v) {
		return k + v;
	});

	auto input = item.Input();
	for (int i = 0; i < 1000; i++) {
		input->Push(i);
	}
	input->Close();
}
