#include <termios.h>
#include <unistd.h>
#include <system_error>
#include <iostream>
#include <string>

class Readline {
    private:
        std::istream & stream_;

        char completion_char_ = '\t';
        std::function<std::string(const std::string &)> completer_;
        std::function<std::string(void)> prompter_;

    public:
        Readline(std::istream &stream): stream_{stream} {}
        std::string read(void) { return ""; }

};

struct EscapeSequence {
    static const std::string ClearTheScreen;
    static const std::string MoveCursorBackward;
    static const std::string MoveCursorForward;
};

using namespace std::literals;

const std::string EscapeSequence::ClearTheScreen{"\x1b[2J"s};
const std::string EscapeSequence::MoveCursorBackward{"\x1b[1D"s};
const std::string EscapeSequence::MoveCursorForward{"\x1b[1C"s};

namespace {
    termios get_terminal_attr(void) {
        termios term;

        if (int rv = tcgetattr(STDIN_FILENO, &term); rv)
            throw std::system_error{errno, std::generic_category()};

        return term;
    }

    void set_terminal_attr(const termios term) {
        if (int rv = tcsetattr(STDIN_FILENO, TCSANOW, &term); rv)
            throw std::system_error{errno, std::generic_category()};
    }
}

class TerminalSettings {
    private:
        termios original_ = get_terminal_attr();
        termios current_ = get_terminal_attr();

    public:
        void apply() const {
            set_terminal_attr(current_);
        }

        void reset() {
            set_terminal_attr(original_);
            current_ = original_;
        }

        TerminalSettings &set_echo(bool to) {
            current_.c_lflag &= to ? ECHO : ~ECHO;
            return *this;
        }

        TerminalSettings &set_canonical(bool to) {
            current_.c_lflag &= to ? ICANON : ~ICANON;
            return *this;
        }

        TerminalSettings &set_min_chars_for_non_canonical_read(size_t n) {
            current_.c_cc[VMIN] = n;
            return *this;
        }

        TerminalSettings &set_timeout_for_non_canonical_read(size_t n) {
            current_.c_cc[VTIME] = n;
            return *this;
        }

        TerminalSettings &set_ctrlc_ctrlz_as_characters(bool to) {
            current_.c_lflag &= to ? ~ISIG : ISIG;
            return *this;
        }

};

class Terminal {
    private:
        TerminalSettings settings_;

        static void write_sequence(const std::string &sequence) {
            ssize_t written = write(STDOUT_FILENO,
                                    sequence.c_str(),
                                    sequence.size());

            if (written == -1) {
                throw std::system_error{errno, std::generic_category()};
            }

            if (static_cast<size_t>(written) != sequence.size()) {
                throw std::runtime_error{"Not enough bytes was written"};
            }
        }

    public:
        Terminal(const TerminalSettings &settings): settings_{settings} {
            settings_.apply();
        }

        ~Terminal() {
            // NOTE: this will abort a program if an exception is raised
            settings_.reset();
        }

        void cursor_forward() const {
        }

        void move_cursor_backward() const {
            write_sequence(EscapeSequence::MoveCursorBackward);
        }

        void clear_the_screen() const {
            write_sequence(EscapeSequence::ClearTheScreen);
        }
};


char constexpr ctrl_key(char c) {
    return c & 0x1F;
}

int main(int argc, char **argv) {

    auto settings = TerminalSettings()
        .set_echo(false)
        .set_canonical(false)
        .set_ctrlc_ctrlz_as_characters(true)
        .set_timeout_for_non_canonical_read(0)
        .set_min_chars_for_non_canonical_read(1);

    auto term = Terminal{settings};
    auto readline = Readline(std::cin);

    while (int t = std::cin.get()) {

        switch (t) {
            case ctrl_key(3):
                goto done;
            case ctrl_key(4):
                // term.clear_the_screen();
                term.move_cursor_backward();
                break;
            default:
                std::cout << int{t} << std::flush;
        }
    }
done:
    ;
}
