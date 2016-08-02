#include "catch.hpp"

#include "stream.hpp"

#include <iostream>

TEST_CASE("TestPoolProcessing") {
	std::cout << "TestPoolProcessing -> " << std::endl;

    using namespace concurrent;

	Streamer<int> item;
    auto result = item.Map<std::multimap<int, int>, int, int, int>([] (int i) {
        return std::move(std::pair<int, int>(i, i));
    })->Collect<std::multimap<int, int>, int, int, int>([](int k, int v) {
		return k + v;
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
	auto result = item.Filter<int>([](int k) {
		return k < 50;
	})->Map<std::map<int, int>, int, int, int>([](int i) {
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
	auto result = Streamer<Test>(input.begin(), input.end()).Filter<Test>([](Test k) {
		return k.status == true;
	}, 4)->Map<std::map<int64_t, Test>, Test, int64_t, Test>([](Test t) {
		return std::move(std::pair<int64_t, Test>(t.id, t));
	}, 4);

	auto count = result->Reduce<int64_t, Test, size_t>([] (int64_t v, Test t, size_t& s) {
		return s + 1;
	});
	REQUIRE(count == 50000);

	auto output = result->Output();
	result->Close();
	REQUIRE(output->Size() == 50000);

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
	auto ret = item.Reduce<int, int>([](int i, int& res) {
		return res + i;
	});

	REQUIRE(ret == 10);

	item.Stream(input.begin(), input.end());
	auto ret2 = item.Map<std::map<int, int>, int, int, int>([](int t) {
		return std::move(std::make_pair(t, t));
	})->Reduce<int, int, int>([](int k, int v, int& o) {
		return o + 1;
	});

	REQUIRE(ret2 == 4);

	std::cout << "<- TestPoolConsumer" << std::endl;
}