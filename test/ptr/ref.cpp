#include <zero/ptr/ref.h>
#include <catch2/catch_test_macros.hpp>

struct A : zero::ptr::RefCounter {

};

struct B : A {
    int x{1024};
};

TEST_CASE("smart pointer using intrusive reference counting", "[ref]") {
    zero::ptr::RefPtr<B> b = zero::ptr::makeRef<B>();

    REQUIRE(b);
    REQUIRE(b.useCount() == 1);

    SECTION("constructor") {
        zero::ptr::RefPtr<B> b1 = b;
        REQUIRE(b1);
        REQUIRE(b1.useCount() == 2);

        zero::ptr::RefPtr<B> b2 = std::move(b1);
        REQUIRE(b2);
        REQUIRE(b2.useCount() == 2);

        zero::ptr::RefPtr<A> a = b2;
        REQUIRE(a);
        REQUIRE(a.useCount() == 3);

        zero::ptr::RefPtr<A> a1 = std::move(b2);
        REQUIRE(a1);
        REQUIRE(a1.useCount() == 3);
    }

    SECTION("assignment") {
        zero::ptr::RefPtr<B> b1;
        REQUIRE(!b1);

        b1 = b;
        REQUIRE(b1);
        REQUIRE(b1.useCount() == 2);

        zero::ptr::RefPtr<B> b2;
        REQUIRE(!b2);

        b2 = std::move(b1);
        REQUIRE(b2);
        REQUIRE(b2.useCount() == 2);

        zero::ptr::RefPtr<A> a;
        REQUIRE(!a);

        a = b2;
        REQUIRE(a);
        REQUIRE(a.useCount() == 3);

        zero::ptr::RefPtr<A> a1;
        REQUIRE(!a1);

        a1 = std::move(b2);
        REQUIRE(a1);
        REQUIRE(a1.useCount() == 3);
    }

    SECTION("methods") {
        REQUIRE(b.get()->x == 1024);
        REQUIRE(b->x == 1024);
        REQUIRE((*b).x == 1024);

        zero::ptr::RefPtr<B> b1;
        REQUIRE(!b1);
        REQUIRE(b1.useCount() == 0);

        b1.swap(b);
        REQUIRE(b1);
        REQUIRE(!b);

        REQUIRE(b1.get()->x == 1024);
        REQUIRE(b1->x == 1024);
        REQUIRE((*b1).x == 1024);
        REQUIRE(b1.useCount() == 1);

        b.reset(b1.get());
        REQUIRE(b.useCount() == 2);

        b1.reset();
        REQUIRE(!b1);
        REQUIRE(b1.useCount() == 0);
        REQUIRE(b.useCount() == 1);
    }
}
