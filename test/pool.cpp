#include "catch.hpp"

#include "pool.hpp"

#include <iostream>
#include <assert.h>


TEST_CASE("TestPool") {
	std::cout << "TestPool -> " << std::endl;

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

	std::cout << "<- TestPool" << std::endl;
}

TEST_CASE("TestPoolInit") {
	std::cout << "TestPoolInit -> " << std::endl;

    std::atomic_int val;

    concurrent::Pool<void, int, std::string> simple([] (int a, std::string b){
        //REQUIRE(b == "test");

    });

    for (int i = 0; i < 10; i++) {
        simple.Call(1, std::string("test"));
    }

	std::cout << "<- TestPoolInit" << std::endl;
}

TEST_CASE("TestPoolPostProcess") {
	std::cout << "TestPoolPostProcess -> " << std::endl;

    concurrent::Pool<double> simple;

	std::function<double (int, std::string)> fun = [](int a, std::string b) {
		assert(a == 1);
		assert(b == "test");
		return a*2.0f;
	};

	std::function<void (double)> cb = [](double a) {
		assert(a == 2.0f);
	};

    simple.Send<int, std::string>(fun, cb, 1, std::string("test"));

	std::cout << "<- TestPoolPostProcess" << std::endl;
}

TEST_CASE("TestPoolSpawn") {
	std::cout << "TestPoolSpawn -> " << std::endl;

    concurrent::Pool<> simple(2);

    //CHECK(simple.Size() == 2);

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

    //REQUIRE(simple.Size() == 4);

	std::cout << "<- TestPoolSpawn" << std::endl;
}

TEST_CASE("TestPoolDefault", "DefaultPool") {
	std::cout << "TestPoolDefault -> " << std::endl;

    concurrent::SystemTaskPool<>().Spawn(
        [] (){
            while (concurrent::SystemTaskPool<>().IsRunning()) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
            std::cout << "Shutting down default pool.." << std::endl;
        }
    );
}

