#define BOOST_TEST_MODULE CppReadline
#define BOOST_TEST_DYN_LINK

#include <boost/test/unit_test.hpp>
#include <sstream>
#include "../src/readline.hh"

BOOST_AUTO_TEST_SUITE(TestCommandReader)

BOOST_AUTO_TEST_CASE(SimpleCommandWorks) {

    std::stringstream input{"a"};
    CommandReader<char> reader{input};
    bool command_called = false;

    reader.add_command('a', [&] { command_called = true; });
    reader.start_reading();
    reader.read_and_execute();

    BOOST_CHECK_EQUAL(command_called, true);
}

BOOST_AUTO_TEST_CASE(TheLongestCommandMatches) {

    std::stringstream input{"abc"};
    CommandReader<char> reader{input};
    bool command_a_called = false,
         command_b_called = false,
         command_c_called = false;

    reader.add_command('a', [&] { command_a_called = true; });
    reader.add_command({'a','b'}, [&] { command_b_called = true; });
    reader.add_command({'a', 'b', 'c'}, [&] { command_c_called = true; });

    reader.start_reading();
    reader.read_and_execute();

    BOOST_CHECK_EQUAL(command_a_called, false);
    BOOST_CHECK_EQUAL(command_b_called, false);
    BOOST_CHECK_EQUAL(command_c_called, true);
}

BOOST_AUTO_TEST_CASE(DefaultCommandWorks) {

    std::stringstream input{"x"};
    CommandReader<char> reader{input};
    bool default_command_called = false,
         command_a_called = false;

    reader.set_default([&] { default_command_called = true; });
    reader.add_command('a', [&] { command_a_called = true; });

    reader.start_reading();
    reader.read_and_execute();

    BOOST_CHECK_EQUAL(default_command_called, true);
    BOOST_CHECK_EQUAL(command_a_called, false);
}

BOOST_AUTO_TEST_CASE(EmptyInputStopsReading) {

    std::stringstream input{""};
    CommandReader<char> reader{input};
    reader.start_reading();
    reader.read_and_execute();

    BOOST_CHECK_EQUAL(reader.current_char(), EOF);
}

BOOST_AUTO_TEST_CASE(ItCanReadConsecutiveCommands) {

    std::stringstream input{"ab"};
    CommandReader<char> reader{input};
    bool command_a_called = false,
         command_b_called = false;

    reader.add_command('a', [&] { command_a_called = true; });
    reader.add_command('b', [&] { command_b_called = true; });

    reader.start_reading();
    reader.read_and_execute();

    BOOST_CHECK_EQUAL(command_a_called, true);
    BOOST_CHECK_EQUAL(command_b_called, true);
}

BOOST_AUTO_TEST_CASE(ItCanReadConsecutiveCommandsWithDefault) {

    std::stringstream input{"abcde"};
    CommandReader<char> reader{input};
    bool command_a_called = false,
         command_b_called = false;
    unsigned int default_called = 0;

    reader.add_command('a', [&] { command_a_called = true; });
    reader.add_command('b', [&] { command_b_called = true; });
    reader.set_default([&] { default_called++; });

    reader.start_reading();
    reader.read_and_execute();

    BOOST_CHECK_EQUAL(command_a_called, true);
    BOOST_CHECK_EQUAL(command_b_called, true);
    BOOST_CHECK_EQUAL(default_called, 3);
}

BOOST_AUTO_TEST_SUITE_END()
