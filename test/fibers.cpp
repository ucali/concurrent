#include "catch.hpp"

#ifdef U_WITH_FIBER

#include "fiber.hpp"

TEST_CASE("TestFiberInit") {
	std::cout << "TestFiberInit -> " << std::endl;
	concurrent::FiberStream fiber(4);

	int i = 0;
	fiber.Send([&i](){
		while (i < 1000) {
			i++;
			concurrent::yield();
		}
	});

	int j = 0;
	fiber.Send([&j]() {
		while (j < 1000) {
			j++;
			concurrent::yield();
		}
	});


	fiber.Close();

	REQUIRE(i == 1000);
	REQUIRE(j == i);
	std::cout << "<- TestFiberInit" << std::endl;
}

TEST_CASE("TestFiberSingleThread") {
	std::cout << "TestFiberInit -> " << std::endl;
	concurrent::FiberStream fiber(1);

	int i = 0;
	fiber.Send([&i]() {
		while (i < 100000) {
			i++;
			concurrent::yield();
		}
	});

	int j = 0;
	fiber.Send([&j]() {
		while (j < 100000) {
			j++;
			concurrent::yield();
		}
	});


	fiber.Close();

	REQUIRE(i == 100000);
	REQUIRE(j == i);
	std::cout << "<- TestFiberInit" << std::endl;
}

TEST_CASE("TestFiberQueue") {
	std::cout << "TestFiberInit -> " << std::endl;
	concurrent::FiberStream fiber(1);

	int i = 0;
	fiber.Send([&i]() {
		while (i < 100000) {
			i++;
			concurrent::yield();
		}
	});

	int j = 0;
	fiber.Send([&j]() {
		while (j < 100000) {
			j++;
			concurrent::yield();
		}
	});


	fiber.Close();

	REQUIRE(i == 100000);
	REQUIRE(j == i);
	std::cout << "<- TestFiberInit" << std::endl;
}


#endif // U_WITH_FIBER
