#include <iostream>
#include "readline.hh"

int main() {

    auto settings = TerminalSettings()
        .set_echo(false)
        .set_canonical(false)
        .set_output_processing(false)
        .set_ctrlc_ctrlz_as_characters(true)
        .set_timeout_for_non_canonical_read(0)
        .set_min_chars_for_non_canonical_read(1);

    Readline readline{};

    readline.set_terminal_settings(settings);
    readline.set_prompter([] { return "$> "; });

    for (auto line = readline.read(); !line.empty(); line = readline.read()) {
        std::cout << "got: " << line << std::endl;
    }
}
