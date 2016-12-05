#include "catch.hpp"

#ifdef U_WITH_BOOST

#include "fiber.hpp"

TEST_CASE("TestFiberInit") {
	std::cout << "TestFiberInit -> " << std::endl;
	concurrent::FiberScheduler fiber;
	REQUIRE(fiber.ThreadNum() == 1);

	int i = 0;
	fiber.Run([&i](){
		while (i < 1000) {
			i++;
			concurrent::yield();
		}
	});

	int j = 0;
	fiber.Run([&j]() {
		while (j < 1000) {
			j++;
			concurrent::yield();
		}
	});


	REQUIRE(fiber.ThreadNum() == 1);
	fiber.Close();

	REQUIRE(i == 1000);
	REQUIRE(j == i);
	std::cout << "<- TestFiberInit" << std::endl;
}

TEST_CASE("TestFiberThread") {
	std::cout << "TestFiberThread -> " << std::endl;
	concurrent::FiberScheduler fiber(4);
	REQUIRE(fiber.ThreadNum() == 4);

	int i = 0;
	fiber.Run([&i]() {
		while (i < 100000) {
			i++;
			concurrent::yield();
		}
	});

	int j = 0;
	fiber.Run([&j]() {
		while (j < 100000) {
			j++;
			concurrent::yield();
		}
	});


	REQUIRE(fiber.ThreadNum() == 4);
	fiber.Close();

	REQUIRE(i == 100000);
	REQUIRE(j == i);
	std::cout << "<- TestFiberThread" << std::endl;
}

TEST_CASE("TestFiberProc") {
	std::cout << "TestFiberProc -> " << std::endl;
	concurrent::FiberScheduler fiber(4);
	REQUIRE(fiber.ThreadNum() == 4);

	int i = 0;
	fiber.Run([&i]() {
		while (i < 100000) {
			i++;
			concurrent::yield();
		}
	});

	int k{0};
	while (k++ < 100) {
		fiber.Run([]() {
			concurrent::yield();
			std::this_thread::sleep_for(5*std::chrono::milliseconds(1));
		});
	}

	int j = 0;
	fiber.Run([&j]() {
		while (j < 100000) {
			j++;
			concurrent::yield();
		}
	});


	REQUIRE(fiber.ThreadNum() == 4);
	fiber.Close();

	REQUIRE(i == 100000);
	REQUIRE(j == i);
	std::cout << "<- TestFiberProc" << std::endl;
}

TEST_CASE("TestFiberQueue") {
	std::cout << "TestFiberQueue -> " << std::endl;
	concurrent::FiberScheduler fibers;
	REQUIRE(fibers.ThreadNum() == 1);

	concurrent::Channel<int> chan;

	int i = 0;
	fibers.Run([&i, &chan]() {
		concurrent::ChannelStatus status = concurrent::ChannelStatus::empty;
		while (status != concurrent::ChannelStatus::closed) {
			status = chan.try_pop(i);
		}
		std::cout << "Quit receive" << std::endl;
	});

	int k{0};
	while (k++ < 50) {
		fibers.Run([]() {
			std::this_thread::sleep_for(5*std::chrono::milliseconds(10));
			concurrent::yield();
			std::this_thread::sleep_for(5*std::chrono::milliseconds(10));
		});
	}


	int j = 0;
	fibers.Run([&j, &chan]() {
		while (j < 100000) {
			j++;
			chan.push(j);
		}
		chan.close();
		std::cout << "Quit send" << std::endl;
	});


	REQUIRE(fibers.ThreadNum() == 1);
	fibers.Close();

	REQUIRE(i == 100000);
	REQUIRE(j == i);
	std::cout << "<- TestFiberQueue" << std::endl;
}


#endif // U_WITH_FIBER
