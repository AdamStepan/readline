#include <termios.h>
#include <unistd.h>
#include <system_error>
#include <iostream>
#include <string>
#include <functional>
#include <iomanip>

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
const std::string EscapeSequence::MoveCursorForward{"\x1b[{N}C"s};
const std::string EscapeSequence::MoveCursorHorizonalAbsolute{"\x1b[{N}G"s};

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
        TerminalSettings &settings_;

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
        Terminal(TerminalSettings &settings): settings_{settings} {
            settings_.apply();
        }

        ~Terminal() {
            // NOTE: this will abort a program if an exception is raised
            settings_.reset();
        }

        void move_cursor_forward() const {
            return move_cursor_forward(1);
        }

        void move_cursor_forward(size_t n) const {
            auto sequence{EscapeSequence::MoveCursorForward};
            sequence.replace(sequence.find("{N}"s), 3, std::to_string(n));

            write_sequence(sequence);
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
            move_cursor_horizontal_absolute(0);
        }

        void move_cursor_horizontal_absolute(size_t n) const {
            auto sequence{EscapeSequence::MoveCursorHorizonalAbsolute};
            sequence.replace(sequence.find("{N}"s), 3, std::to_string(n));

            write_sequence(sequence);
        }

        void cooked() { settings_.reset(); }
        void raw() { settings_.apply(); }

};

class Readline {
    private:
        std::string buffer_{};
        size_t position_{0};

        std::istream &input_;
        std::ostream &output_;
        TerminalSettings settings_;

        std::function<std::string(std::string)> completion_;

    protected:
        void write_single_char(const Terminal &term, int c) {

            // we are inserting to the end of string
            if (position_ == buffer_.size()) {
                buffer_.push_back(c);
                output_ << static_cast<char>(c) << std::flush;

                position_++;
            } else {
                // TODO: ensure that we have space for a new character
                // TODO: check capacity() * 2 < max_size()
                term.move_cursor_horizontal_absolute();

                auto ending = buffer_.substr(position_);
                buffer_.erase(position_);

                buffer_.insert(position_, 1, c);

                position_++;
                buffer_.insert(position_, ending);

                output_ << buffer_ << std::flush;
                term.move_cursor_horizontal_absolute(position_ + 1);
            }
        }

        void handle_special_character(const Terminal &term, char c) {

            // XXX: we can use trie with handlers for this
            switch (c) {
                // ctrl + c
                case 3:
                    buffer_.clear();
                    position_ = 0;
                    term.move_cursor_horizontal_absolute(0);
                    term.clear_the_line();
                    return;
            }

            switch (auto c = input_.get(); c) {
                case '[':
                    std::cout << "[, breaking" << std::endl;
                    break;
                case '\x1c':
                    output_ << position_ << " size: " << buffer_.size() << std::endl;
                    return;
                default:
                    std::cout << "unget: -->" << c << "<---" << std::endl;
                    input_.unget();
                    return;
            }

            switch (auto c = input_.get(); c) {
                // move left
                case 'D':
                    if (position_) {
                        --position_;
                        term.move_cursor_backward();
                    }
                    break;
                // move right
                case 'C':
                    if (position_ < buffer_.size()) {
                        ++position_;
                        term.move_cursor_forward();
                    }
                    break;
                case 'A':
                case 'B':
                    // move forward/backward in the history
                    break;
                default:
                    std::cout << "X: " << c << std::endl;
            }
        }

        void do_autocomplete() {
            if (completion_) {
                buffer_ = completion_(buffer_);
            }
        }

    public:
        Readline(const TerminalSettings &s, std::istream &is, std::ostream &os):
            input_{is}, output_{os}, settings_{s}, completion_{} {}

        Readline(const TerminalSettings &s, std::istream &is, std::ostream &os,
                std::function<std::string(std::string)> c):
            input_{is}, output_{os}, settings_{s}, completion_{c} {}

        std::string read(void) {

            Terminal term{settings_};

            while (auto c = input_.get()) {
                if (c == '\n') {
                    std::cout << std::endl;
                    term.move_cursor_horizontal_absolute();
                    return buffer_;
                } else if (c == '\t') {
                    do_autocomplete();
                } else if (iscntrl(c)) {
                    handle_special_character(term, c);
                } else {
                    write_single_char(term, c);
                }

            }

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

    auto readline = Readline(settings, std::cin, std::cout);
    auto line = readline.read();

    std::cout << "got: " << line << std::endl;
}
