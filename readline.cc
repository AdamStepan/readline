#include <termios.h>
#include <unistd.h>
#include <system_error>
#include <iostream>
#include <string>
#include <functional>
#include <iomanip>
#include <unordered_map>
#include <deque>
#include <iterator>
#include <memory>

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

class History {
    std::shared_ptr<std::ostream> history_file_{};
    size_t max_entries_{1024};
    std::deque<std::string> entries_{};

    void write_to_file() {
        std::copy(entries_.cbegin(),
                  entries_.cend(),
                  std::ostream_iterator<std::string>{*history_file_.get(), "\n"});
    }

public:

    std::string get_line(const size_t n) const {
        return entries_.at(n);
    }

    void add_line(const std::string &line) {

        entries_.push_back(line);

        if (entries_.size() > max_entries_) {
            entries_.pop_front();
        }
    }

    size_t size() const {
        return entries_.size();
    }

    bool empty() const {
        return !size();
    }

    void save() {
        write_to_file();
    }
};

class HistoryView {
    const History &history_;
    size_t current_line_{0};

public:
    HistoryView(const History &h): history_{h} {}
    HistoryView(const HistoryView &) = delete;

    const std::string previous() {

        if (history_.empty()) {
            return "";
        }

        if (current_line_) {
            current_line_--;
            return history_.get_line(current_line_);
        } else {
            return history_.get_line(0);
        }

    }

    const std::string next() {

        if (history_.empty()) {
            return "";
        }

        if (current_line_ < history_.size()) {
            return history_.get_line(current_line_++);
        }

        return history_.get_line(history_.size() - 1);
    }
};

class Prompt {
    std::function<std::string(void)> prompter_{};
    std::string prompt_{""};
public:

    void set_prompt(decltype(prompter_) p) {
        prompter_ = p;
    }

    const std::string &operator() () {
        prompt_ = prompter_();
        return prompt_;
    }

    size_t size() const {
        return prompt_.size();
    }

    operator bool () const {
        return static_cast<bool>(prompter_);
    }
};

class Buffer {
    size_t cursor_pos_{0};
    std::string data_;

public:
    void insert(char c) {
        if (cursor_pos_ == data_.size()) {
            data_.push_back(c);
            cursor_pos_++;
        } else {
            // TODO: check size
            auto ending = data_.substr(cursor_pos_);
            data_.erase(cursor_pos_);

            data_.insert(cursor_pos_, 1, c);

            cursor_pos_++;
            data_.insert(cursor_pos_, ending);
        }
    }

    void move_left() {
        cursor_pos_--;
    }

    void move_right() {
        cursor_pos_++;
    }

    void pop_back() {
        cursor_pos_--;
        data_.pop_back();
    }

    void clear() {
        cursor_pos_ = 0;
        data_.clear();
    }

    void reset(std::string s) {
        data_ = std::move(s);
        cursor_pos_ = data_.size();
    }

    size_t position() const {
        return cursor_pos_;
    }

    std::string data() const {
        return data_;
    }

    bool empty() const {
        return data_.empty();
    }

    size_t size() const {
        return data_.size();
    }

    friend std::ostream &operator<<(std::ostream &, const Buffer &);
};

std::ostream &operator<<(std::ostream &os, const Buffer &b) {
    return os << b.data_;
}

class TerminalBuffer {};


class Readline {
    private:
        Buffer buffer_;

        std::reference_wrapper<std::istream> input_{std::cin};
        std::reference_wrapper<std::ostream> output_{std::cout};

        History history_{};
        HistoryView history_view_{history_};

        TerminalSettings settings_{};

        Prompt prompter_{};

        std::function<std::string(std::string)> completion_{};
    protected:
        void write_single_char(const Terminal &term, int c) {

            const size_t position_after_prompt_ = prompter_.size() + 1;

            term.move_cursor_horizontal_absolute(position_after_prompt_);

            buffer_.insert(static_cast<char>(c));
            output_.get() << buffer_ << std::flush;

            term.move_cursor_horizontal_absolute(position_after_prompt_ + buffer_.position());
        }

        using StopFlag = bool;

        void clear_line_without_prompt(const Terminal &term) {
            buffer_.clear();
            term.move_cursor_horizontal_absolute(prompter_.size() + 1);
            term.clear_the_line();
        }

        enum class FirstByte: char {
            CTRL_C = 3,
            CTRL_D = 4,
            CTRL_U = 21,
            BACKSPACE = 127
        };

        StopFlag handle_special_character(const Terminal &term, char c) {

            // XXX: we can use trie with handlers for this
            switch (static_cast<FirstByte>(c)) {
                // TODO: handle all ctrl+ characters in this switch
                case FirstByte::CTRL_C:
                case FirstByte::CTRL_U:
                    clear_line_without_prompt(term);
                    return false;
                case FirstByte::CTRL_D:
                    return buffer_.empty();
                case FirstByte::BACKSPACE:
                    if (buffer_.position()) {
                        buffer_.pop_back();
                        term.move_cursor_backward();
                        term.clear_the_line();
                    }
                    return false;
            }

            switch (auto c = input_.get().get(); c) {
                case '[':
                    break;
                default:
                    std::cerr << "uknown code: " << c << std::endl;
                    input_.get().unget();
                    return true;
            }

            switch (auto c = input_.get().get(); c) {
                // move left
                case 'D':
                    if (buffer_.position()) {
                        buffer_.move_left();
                        term.move_cursor_backward();
                    }
                    break;
                // move right
                case 'C':
                    if (buffer_.position() < buffer_.size()) {
                        buffer_.move_right();
                        term.move_cursor_forward();
                    }
                    break;
                case 'B':
                    if (history_.size()) {
                        clear_line_without_prompt(term);
                        buffer_.reset(history_view_.previous());
                        output_.get() << buffer_;
                    }
                    break;
                case 'A':
                    // TODO: we should be able to get back to the empty line
                    if (history_.size()) {
                        clear_line_without_prompt(term);
                        buffer_.reset(history_view_.next());
                        output_.get() << buffer_;
                    }
                    break;
                default:
                    break;
            }

            return false;
        }

        void do_autocomplete() {
            if (completion_) {
                // XXX: we should pass some completion info
                buffer_.reset(completion_(buffer_.data()));
            }
        }

        void do_print_prompt() {
            if (prompter_) {
                output_.get() << prompter_();
            }
        }

        void add_history() {
            history_.add_line(buffer_.data());
        }
    public:

        std::string read(void) {

            Terminal term{settings_};
            buffer_.clear();
            term.move_cursor_horizontal_absolute();
            do_print_prompt();

            while (auto c = input_.get().get()) {
                if (c == '\n') {
                    std::cout << std::endl;
                    term.move_cursor_horizontal_absolute();
                    goto quit;
                } else if (c == '\t') {
                    do_autocomplete();
                } else if (iscntrl(c)) {
                    if(handle_special_character(term, c)) {
                        break;
                    }
                } else {
                    write_single_char(term, c);
                }

            }
quit:
            add_history();
            return buffer_.data();
        }

        Readline &set_terminal_settings(const TerminalSettings &s) {
            settings_ = s;
            return *this;
        }

        Readline &set_output_stream(std::ostream &os) {
            output_ = os;
            return *this;
        }

        Readline &set_input_stream(std::istream &is) {
            input_ = is;
            return *this;
        }

        Readline &set_autocomplete(std::function<std::string(std::string)> c) {
            completion_ = c;
            return *this;
        }

        Readline &set_prompter(std::function<std::string(void)> p) {
            prompter_.set_prompt(p);
            return *this;
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

    Readline readline{};
    readline.set_terminal_settings(settings).set_prompter([] { return "$> "; });

    for (auto line = readline.read(); !line.empty(); line = readline.read()) {
        std::cout << "got: " << line << std::endl;
    }
}
