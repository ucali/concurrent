#include "catch.hpp"

#include "pool.hpp"

#include <iostream>

TEST_CASE("TestPoolSimple") {
    concurrent::Pool<> simple;

    simple.Send<int, std::string>(
        [] (int a, std::string b){
            CHECK(a == 1);
            CHECK(b == "test");
        }, 1, std::string("test")
    );

    simple.Send<std::string, int>(
        [] (std::string b, int a, double val){
            CHECK(a == 1);
            CHECK(b == "test");
            CHECK(val == 0.1);
        }, std::string("test"), 1, 0.1
    );

    simple.Close();
}

TEST_CASE("TestPoolPostProcessSimple") {
    concurrent::Pool<double> simple;

    simple.Send<int, std::string>(
        [] (auto a, auto b){
            CHECK(a == 1);
            CHECK(b == "test");
            return a*2.0f;
        }, [] (auto a) {
            CHECK(a == 2.0f);
        }, 1, std::string("test")
    );
}

TEST_CASE("TestPoolPushSimple") {
    concurrent::Pool<> simple(2);

    CHECK(simple.Size() == 2);

    simple.Push<int, std::string>(
        [] (int a, std::string& b){
            CHECK(a == 1);
            CHECK(b == "test");
        }, 1, std::string("test")
    );

    CHECK(simple.Size() == 3);
}
