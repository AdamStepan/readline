#define BOOST_TEST_MODULE CppReadline
#define BOOST_TEST_DYN_LINK

#include <boost/test/unit_test.hpp>
#include <sstream>
#include "../src/readline.hh"

using namespace std::literals;

BOOST_AUTO_TEST_SUITE(TestBuffer)

BOOST_AUTO_TEST_CASE(TestInsertAtTheEnd) {

    Buffer buffer{};
    buffer.insert('a');

    BOOST_CHECK_EQUAL(buffer.position(), 1);
    BOOST_CHECK_EQUAL(buffer.data(), "a"s);
    BOOST_CHECK_EQUAL(buffer.empty(), false);
}

BOOST_AUTO_TEST_CASE(TestInsertWithLeftMove) {

    Buffer buffer{};
    for (auto ch: {'a', 'b', 'c', 'd'}) {
        buffer.insert(ch);
    }

    BOOST_CHECK_EQUAL(buffer.position(), 4);
    BOOST_CHECK_EQUAL(buffer.data(), "abcd"s);

    buffer.move_left();
    buffer.move_left();

    BOOST_CHECK_EQUAL(buffer.position(), 2);
    buffer.insert('x');

    BOOST_CHECK_EQUAL(buffer.position(), 3);
    BOOST_CHECK_EQUAL(buffer.data(), "abxcd"s);
}

BOOST_AUTO_TEST_CASE(TestEraseAtTheEnd) {

    Buffer buffer{};
    for (auto ch: {'a', 'b', 'c', 'd'}) {
        buffer.insert(ch);
    }

    BOOST_CHECK_EQUAL(buffer.position(), 4);
    BOOST_CHECK_EQUAL(buffer.data(), "abcd"s);

    buffer.remove();

    BOOST_CHECK_EQUAL(buffer.position(), 3);
    BOOST_CHECK_EQUAL(buffer.data(), "abc"s);
}

BOOST_AUTO_TEST_CASE(TestEraseInTheMiddle) {

    Buffer buffer{};
    for (auto ch: {'a', 'b', 'c', 'd'}) {
        buffer.insert(ch);
    }

    BOOST_CHECK_EQUAL(buffer.position(), 4);
    BOOST_CHECK_EQUAL(buffer.data(), "abcd"s);

    buffer.move_left();
    buffer.remove();

    BOOST_CHECK_EQUAL(buffer.position(), 2);
    BOOST_CHECK_EQUAL(buffer.data(), "abd"s);
}

BOOST_AUTO_TEST_CASE(TestEraseEmptyBuffer) {

    Buffer buffer{};
    buffer.remove();

    BOOST_CHECK_EQUAL(buffer.position(), 0);
}

BOOST_AUTO_TEST_CASE(TestMovesWithEmptyBuffer) {

    Buffer buffer{};

    buffer.move_left();
    BOOST_CHECK_EQUAL(buffer.position(), 0);

    buffer.move_right();
    BOOST_CHECK_EQUAL(buffer.position(), 0);
}

BOOST_AUTO_TEST_SUITE_END()
