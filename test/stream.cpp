#include "catch.hpp"

#include "stream.hpp"

#include <unordered_map>

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

	REQUIRE(result->Output()->Size() == 2000);

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
	REQUIRE(result->Output()->Size() == 50);

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
		s++;
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
		res += i;
	});

	REQUIRE(ret == 10);

	item.Stream(input.begin(), input.end());
	auto ret2 = item.KV<std::map<int, int>>([](int t) {
		return std::move(std::make_pair(t, t));
	})->Reduce<int>([](auto v, int& o) {
		o++;
	});

	REQUIRE(ret2 == 4);

	std::cout << "<- TestPoolConsumer" << std::endl;
}



TEST_CASE("TestPartition") {
	std::cout << "TestPartition -> " << std::endl;
	using namespace concurrent;

	std::vector<int> input;
	for (int i = 0; i < 1000; i++) {
		for (int j = 0; j < 1000; j++) {
			input.push_back(i + 1);
		}
	}

	size_t v1 = 0, v2 = 0;
	int c1 = 0, c2 = 0;


	concurrent::Pool<>::Ptr pool(new concurrent::Pool<>);
	
	Streamer<int> item(input.begin(), input.end(), pool);
	item.KV<std::multimap<int, int>>([](int t) {
		return std::move(std::make_pair(t, t));
	})->Partition<std::vector<int>, size_t>([](const auto& k, auto vec) {
		return vec->size();
	})->ForEach([&v1, &c1](auto v) {
		v1 += v;
		c1++;
	});

	Streamer<int> item1(input.begin(), input.end(), pool);
	item1.KV<std::unordered_multimap<int, int>>([](int t) {
		return std::move(std::make_pair(t, t));
	})->PartitionMT<std::vector<int>, size_t>([](const auto& k, auto vec) {
		return vec->size();
	})->ForEach([&v2, &c2](auto v) {
		v2 += v;
		c2++;
	});

	std::cout << v1 << " " << c1 << std::endl;
	std::cout << v2 << " " << c2 << std::endl;
	REQUIRE(v1 == 1000*1000);
	REQUIRE(v1 == v2);

	std::cout << v1 << " <- TestPartition" << std::endl;


}