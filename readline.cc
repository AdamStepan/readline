#include <termios.h>
#include <unistd.h>
#include <system_error>
#include <iostream>
#include <string>
#include <functional>



struct EscapeSequence {
    static const std::string ClearTheScreen;
    static const std::string MoveCursorBackward;
    static const std::string MoveCursorForward;
    static const std::string MoveCursorHorizonalAbsolute;
    static const std::string ClearTheLine;
};

using namespace std::literals;

const std::string EscapeSequence::ClearTheScreen{"\x1b[2J"s};
const std::string EscapeSequence::ClearTheLine{"\x1b[K"s}; // from the active pos to end of the line
const std::string EscapeSequence::MoveCursorBackward{"\x1b[1D"s};
const std::string EscapeSequence::MoveCursorForward{"\x1b[1C"s};
const std::string EscapeSequence::MoveCursorHorizonalAbsolute{"\x1b[G"s};

namespace {
    termios get_terminal_attr(void) {
        termios term;

        if (int rv = tcgetattr(STDIN_FILENO, &term); rv)
            throw std::system_error{errno, std::generic_category()};

        return term;
    }

    void set_terminal_attr(const termios term) {
        if (int rv = tcsetattr(STDIN_FILENO, TCSAFLUSH, &term); rv)
            throw std::system_error{errno, std::generic_category()};
    }
}

class TerminalSettings {
    private:
        termios original_ = get_terminal_attr();
        termios current_ = get_terminal_attr();

    public:
        void apply() {
            set_terminal_attr(current_);
        }

        void reset() {
            set_terminal_attr(original_);
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

        TerminalSettings &set_output_processing(bool to) {
            current_.c_oflag &= to ? ~OPOST : OPOST;
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
        }

        ~Terminal() {
            // NOTE: this will abort a program if an exception is raised
            settings_.reset();
        }

        void move_cursor_forward() const {
            write_sequence(EscapeSequence::MoveCursorForward);
        }

        void move_cursor_backward() const {
            write_sequence(EscapeSequence::MoveCursorBackward);
        }

        void clear_the_screen() const {
            write_sequence(EscapeSequence::ClearTheScreen);
        }

        void clear_the_line() const {
            write_sequence(EscapeSequence::ClearTheLine);
        }

        void move_cursor_horizontal_absolute() const {
            write_sequence(EscapeSequence::MoveCursorHorizonalAbsolute);
        }

        void cooked() { settings_.reset(); }
        void raw() { settings_.apply(); }

};

class Readline {
    private:
        std::string buffer_{};
        size_t position_{0};

        std::istream &stream_;
        Terminal term_;

    protected:
        void write_single_char(int c) {
            buffer_.insert(position_++, 1, static_cast<char>(c));
            std::cout << static_cast<char>(c) << std::flush;
        }

        void handle_special_character() {

            if (auto c = stream_.get(); c != '[') {
                stream_.unget();
                return;
            }

            switch (auto c = stream_.get(); c) {
                case 'D':
                    --position_;
                    term_.move_cursor_backward();
                    break;
                case 'C':
                    ++position_;
                    term_.move_cursor_forward();
                    break;
                case 'A':
                case 'B':
                    // move forward/backward in the history
                    break;
                default:
                    std::cout << "X: " << c << std::endl;
            }
        }
    public:
        Readline(const Terminal &term, std::istream &stream): stream_{stream},
            term_{term} {}

        std::string read(void) {

            term_.raw();

            while (auto c = stream_.get()) {

                if (c == '\n') {
                    std::cout << std::endl;
                    term_.move_cursor_horizontal_absolute();
                    term_.cooked();
                    return buffer_;
                } else if (iscntrl(c)) {
                    handle_special_character();
                } else {
                    write_single_char(c);
                }

            }

            term_.cooked();
            return buffer_;
        }
};

int main(int argc, char **argv) {

    auto settings = TerminalSettings()
        .set_echo(false)
        .set_canonical(false)
        .set_output_processing(false)
        .set_ctrlc_ctrlz_as_characters(true)
        .set_timeout_for_non_canonical_read(0)
        .set_min_chars_for_non_canonical_read(1);

    auto term = Terminal{settings};
    auto readline = Readline(term, std::cin);

    auto line = readline.read();

    std::cout << "got: " << line << std::endl;
}
