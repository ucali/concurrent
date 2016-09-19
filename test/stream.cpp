#include "catch.hpp"

#include "stream.hpp"

#include <iostream>
#include <assert.h>

TEST_CASE("TestPoolProcessing") {
	std::cout << "TestPoolProcessing -> " << std::endl;

    using namespace concurrent;

	Streamer<int> item;
    auto result = item.KV<std::multimap<int, int>>([] (int i) {
        return std::make_pair(i, i);
    })->Transform<int>([](auto k) {
		return k.first + k.second;
	});

	auto input = item.Input();
	for (int i = 0; i < 1000; i++) {
		input->Push(i);
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
	auto result = item.Filter([](int k) {
		return k < 50;
	})->Transform<int>([](auto k) {
		return k;
	})->KV<std::map<int, int>>([](int i) {
		return std::move(std::make_pair(i, i));
	});

	auto input = item.Input();
	for (int i = 0; i < 1000; i++) {
		input->Push(i);
	}
	input->Close();
	result->Close();

	std::cout << "<- TestPoolFilter" << std::endl;
}

TEST_CASE("TestPoolClass") {
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
	input.reserve(10000000);
	for (int i = 0; i < 10000000; i++) {
		input.push_back({ i, i % 2 == 0, "hello world" });
	}

	//Stream some data:
    auto result = Streamer<Test>(input.begin(), input.end()).Filter([](Test k) {
		return k.status == true;
	}, 2)->KV<std::map<int64_t, Test>>([](Test t) {
		return std::make_pair(t.id, t);
	}, 2);

	auto count = result->Reduce<size_t>([] (auto t, size_t& s) {
		return s + 1;
	});
	REQUIRE(count == 5000000);

	auto output = result->Output();
	result->Close();
	REQUIRE(output->Size() == 5000000);

	std::cout << "<- TestPoolSimpleClass" << std::endl;
}


TEST_CASE("TestPoolConsumer") {
	using namespace concurrent;

	std::vector<int> input;
	for (int i = 0; i < 4; i++) {
		input.push_back(i + 1);
	}

	std::cout << "TestPoolConsumer -> " << std::endl;

	Streamer<int> item(input.begin(), input.end());
	auto ret = item.Reduce<int>([](int i, int& res) {
		return res + i;
	});

	REQUIRE(ret == 10);

	item.Stream(input.begin(), input.end());
	auto ret2 = item.KV<std::map<int, int>>([](int t) {
		return std::move(std::make_pair(t, t));
	})->Reduce<int>([](auto v, int& o) {
		return o + 1;
	});

	REQUIRE(ret2 == 4);

	std::cout << "<- TestPoolConsumer" << std::endl;
}



TEST_CASE("TestPartition") {
	std::cout << "TestPartition -> " << std::endl;
	using namespace concurrent;

	std::vector<int> input;
	for (int i = 0; i < 4; i++) {
		input.push_back(i + 1);
		input.push_back(i + 1);
	}

	Streamer<int> item(input.begin(), input.end());
	item.KV<std::multimap<int, int>>([](int t) {
		return std::move(std::make_pair(t, t));
	})->Partition<std::vector<int>>();

	Streamer<int> item1(input.begin(), input.end());
	item1.KV<std::multimap<int, int>>([](int t) {
		return std::move(std::make_pair(t, t));
	})->Partition<std::vector<int>>([] (auto&& vec) {
		assert(vec.size() == 2);
		return true;
	});


	std::cout << "<- TestPartition" << std::endl;
}