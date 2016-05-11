#include "catch.hpp"

#include "pool.hpp"

#include <iostream>
#include <assert.h>


TEST_CASE("TestPoolSimple") {
	std::cout << "TestPoolSimple -> " << std::endl;

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

	std::cout << "<- TestPoolSimple" << std::endl;
}

TEST_CASE("TestPoolSimpleInit") {
	std::cout << "TestPoolSimpleInit -> " << std::endl;

    std::atomic_int val;

    concurrent::Pool<void, int, std::string> simple([] (int a, std::string b){
        //REQUIRE(b == "test");

    });

    for (int i = 0; i < 10; i++) {
        simple.Call(1, std::string("test"));
    }

	std::cout << "<- TestPoolSimpleInit" << std::endl;
}

TEST_CASE("TestPoolPostProcessSimple") {
	std::cout << "TestPoolPostProcessSimple -> " << std::endl;

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

	std::cout << "<- TestPoolPostProcessSimple" << std::endl;
}

TEST_CASE("TestPoolSpawnSimple") {
	std::cout << "TestPoolSpawnSimple -> " << std::endl;

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

	std::cout << "<- TestPoolSpawnSimple" << std::endl;
}

/*TEST_CASE("TestPoolDefault", "DefaultPool") {
	std::cout << "TestMapCallback -> " << std::endl;

    concurrent::SystemTaskPool<>().Spawn(
        [] (){
            while (concurrent::SystemTaskPool<>().IsRunning()) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
            std::cout << "Shutting down default pool.." << std::endl;
        }
    );
}*/

TEST_CASE("TestPoolProcessing") {
	std::cout << "TestPoolProcessing -> " << std::endl;

    using namespace concurrent;

	Streamer<int> item;
    auto result = item.Map<int, int, int>([] (int i) {
        return std::move(std::pair<int, int>(i, i));
    })->Collect<int, int, int>([](int k, int v) {
		return k + v;
	});

	auto input = item.Input();
	for (int i = 0; i < 1000; i++) {
		input->Push(i);
	}
	input->Close();
	result->Close();

	std::cout << "<- TestPoolProcessing" << std::endl;
}

TEST_CASE("TestPoolFilter") {
	std::cout << "TestPoolFilter -> " << std::endl;

	using namespace concurrent;

	Streamer<int> item;
	auto result = item.Filter<int>([](int k) {
		return k < 50;
	})->Map<int, int, int>([](int i) {
		return std::move(std::pair<int, int>(i, i));
	});

	auto input = item.Input();
	for (int i = 0; i < 1000; i++) {
		input->Push(i);
	}
	input->Close();
	result->Output()->Close();
	result->Close();

	std::cout << "<- TestPoolFilter" << std::endl;
}

TEST_CASE("TestPoolSimpleClass") {
	std::cout << "TestPoolSimpleClass -> " << std::endl;

	using namespace concurrent;

	class Test {
	public:
		Test() {}
		Test(int64_t i, bool s, std::string v) : id(i), status(s), val(v) { }

		int64_t id = 0;
		bool status = true;
		std::string val = "hello";
	};

	std::vector<Test> input;
	for (int i = 0; i < 100000; i++) {
		input.push_back({ i, i % 2 == 0, "hello world" });
	}


	//Stream some data:
	Streamer<Test> item;
	auto result = item.Filter<Test>([](Test k) {
		return k.status == true;
	})->Map<Test, int64_t, Test>([](Test t) {
		return std::move(std::pair<int64_t, Test>(t.id, t));
	});

	item.Stream(input);

	auto output = result->Output();
	output->Wait();
	result->Close();
	REQUIRE(output->Size() == 50000);

	std::cout << "<- TestPoolSimpleClass" << std::endl;
}
