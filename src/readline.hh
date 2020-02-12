#pragma once

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
        termios original_{get_terminal_attr()};
        termios current_{get_terminal_attr()};

    public:
        TerminalSettings() {}

        TerminalSettings(const TerminalSettings &s) {
            original_ = s.original_;
            current_ = s.current_;
        }

        void apply() const {
            set_terminal_attr(current_);
        }

        void reset() const {
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

        Terminal() {}

        Terminal(const TerminalSettings &settings): settings_{settings} {}
        Terminal(const Terminal &t): settings_{t.settings_} {}

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

        void set_settings(const TerminalSettings &s) {
            settings_ = s;
        }

        void apply_settings() {
            settings_.apply();
        }

        void reset_settings() {
            settings_.reset();
        }
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
    History history_;
    size_t current_line_{0};

public:

    void add_line(const std::string &line) {
        const size_t current_size = history_.size();
        history_.add_line(line);
        if (current_size < history_.size()) {
            current_line_++;
        }

    }

    void reset_position() {
        current_line_ = history_.size();
    }

    size_t size() const {
        return history_.size();
    }

    bool empty() const {
        return !size();
    }

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

        return ""; //history_.get_line(history_.size() - 1);
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

    void remove() {
        data_.erase(cursor_pos_--, 1);
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

//class TerminalBuffer {
//
//    size_t prompt_end_{0};
//    Buffer &buffer_;
//    Terminal &terminal_;
//
//public:
//    void set_prompt_end(const size_t end) {
//        prompt_end_ = end;
//    }
//};

const auto CTRL_C = 3;
const auto CTRL_D = 4;
const auto CTRL_U = 21;
const auto ESC = '\x1b';
const auto BACKSPACE = '\x7f';
const auto NEWLINE = '\n';
const auto TAB = '\t';

const auto MOVE_LEFT = {ESC, '[', 'D'};
const auto MOVE_RIGHT = {ESC, '[', 'C'};
const auto MOVE_DOWN = {ESC, '[', 'A'};
const auto MOVE_UP = {ESC, '[', 'B'};

template <typename T>
struct Trie {

    using Key = T;
    using SubTrie = std::unique_ptr<Trie<T>>;
    using Item = std::function<void(void)>;

    std::unordered_map<Key, SubTrie> data;
    Item item;

    template <typename It>
    bool insert(It begin, It end, const Item &item) {

        if (begin == end) {
            return false;
        }

        auto &&subtrie = data[*begin];

        if (!subtrie) {
            subtrie.reset(new Trie<T>());
        }

        auto &&inserted = subtrie->insert(std::next(begin), end, item);

        if (!inserted) {
            data[*begin]->item = item;
        }

        return true;
    }

    void insert(const std::initializer_list<const T> &c, const Item &i) {
        insert(std::begin(c), std::end(c), i);
    }

    void insert(const Key &_, const Item &i) {
        auto && k= std::initializer_list<Key>{_};
        insert(std::begin(k), std::end(k), i);
    }

    bool contains(const Key &k) const {
        return data.count(k) >= 1;
    }

    bool empty() const {
        return data.empty();
    }

    const Trie &operator[](const Key &k) const {
        return *data.at(k);
    }

    const Trie &operator[](const std::initializer_list<Key> &keys) const {
        const Trie *subtrie = this;

        for (auto &&key: keys) {
            subtrie = subtrie->data.at(key).get();
        }

        return *subtrie;
    }

    void operator() () const {
        item();
    }
};

#include <vector>

template <typename T>
struct UnknownCommandSequence: std::runtime_error {

    UnknownCommandSequence(const std::vector<T> &s):
        std::runtime_error{mkmsg(s)} {}

    virtual std::string mkmsg(const std::vector<T> &s) = 0;
};

template <>
struct UnknownCommandSequence<char>: std::runtime_error {

    UnknownCommandSequence(const std::vector<char> &s):
        std::runtime_error{mkmsg(s)} {}

    std::string mkmsg(const std::vector<char> &s) {

        std::string msg{"Unknown command sequence: ["};

        for (auto &&it = s.cbegin(); it != s.cend(); it++) {
            msg += std::to_string(static_cast<int>(*it));
            if (std::next(it) != s.cend()) {
                msg += ", ";
                std::prev(it);
            }
        }
        msg += "]";

        return msg;
    }

};
template <typename T>
struct CommandReader {

    Trie<T> commands_;
    typename Trie<T>::Item default_;

    std::istream &input_;

    bool should_stop_{false};
    char c_{'\0'};

    CommandReader(std::istream &is): input_{is} {}

    template <typename F>
    void add_command(const std::initializer_list<const T> &key, F &&f) {
        commands_.insert(key, f);
    }

    template <typename F>
    void add_command(const T &key, F &&f) {
        commands_.insert(key, f);
    }

    template <typename F>
    void set_default(F &&f) {
        default_ = f;
    }

    void stop_reading() { should_stop_ = true; }
    void start_reading() { should_stop_ = false; }
    char current_char() const { return c_; }

    void run_command(const Trie<T> &t) {
        // current character does not have associated command
    }

    void read_and_execute() {

        const Trie<T> *t = &commands_;

        while (!should_stop_) {

            c_ = input_.get();

            if (c_ == EOF) {
                should_stop_ = true;
                break;
            }
            // XXX: THIS is a total bullshit, you should rewrite it
            if (!t->contains(c_)) {
                if (!t->item) {
                    if (default_) {
                        default_();
                    } else {
                        throw std::runtime_error("Unknown command: " + std::to_string((int)c_));
                    }
                } else {
                    t->item();
                    input_.unget();
                }

                t = &commands_;
            } else {
                t = &(*t)[c_];//t->operator[](c_);

                if (t->empty() && t->item) {
                    t->item();
                    t = &commands_;
                }
            }
        }

    }

};

using Completion = std::function<std::string(std::string)>;

class Readline {
    private:
        Buffer buffer_;

        std::reference_wrapper<std::istream> input_{std::cin};
        std::reference_wrapper<std::ostream> output_{std::cout};

        HistoryView history_{};

        TerminalSettings settings_{};
        Terminal terminal_{};

        Prompt prompter_{};
        Completion completion_{};

        CommandReader<char> command_reader_{input_.get()};

    protected:
        void do_write_char() {
            const size_t position_after_prompt_ = prompter_.size() + 1;

            terminal_.move_cursor_horizontal_absolute(position_after_prompt_);

            buffer_.insert(command_reader_.current_char());
            output_.get() << buffer_ << std::flush;

            terminal_.move_cursor_horizontal_absolute(position_after_prompt_ + buffer_.position());
        }

        void do_backspace() {
            if (buffer_.position()) {
                terminal_.move_cursor_horizontal_absolute(prompter_.size() + 1);
                terminal_.clear_the_line();
                buffer_.remove();
                output_.get() << buffer_ << std::flush;
            }
        }

        void do_clear_line() {
            buffer_.clear();
            terminal_.move_cursor_horizontal_absolute(prompter_.size() + 1);
            terminal_.clear_the_line();
        }

        void do_accept_command() {
            output_.get() << std::endl;
            add_history();
            history_.reset_position();
            terminal_.move_cursor_horizontal_absolute();
            command_reader_.stop_reading();
        }

        void do_control_d() {
            if (buffer_.empty()) {
                command_reader_.stop_reading();
            }
        }

        void do_move_left() {
            if (buffer_.position()) {
                buffer_.move_left();
                terminal_.move_cursor_backward();
            }
        }

        void do_move_right() {
            if (buffer_.position() < buffer_.size()) {
                buffer_.move_right();
                terminal_.move_cursor_forward();
            }
        }

        void do_history_up() {
            if (history_.size()) {
                do_clear_line();
                buffer_.reset(history_.previous());
                output_.get() << buffer_;
            }
        }

        void do_history_down() {
            if (history_.size()) {
                do_clear_line();
                buffer_.reset(history_.next());
                output_.get() << buffer_;
            }
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
        Readline() {
            command_reader_.add_command(CTRL_U, [this] { do_clear_line(); });
            command_reader_.add_command(CTRL_C, [this] { do_clear_line(); });
            command_reader_.add_command(BACKSPACE, [this] { do_backspace(); });
            command_reader_.add_command(CTRL_D, [this] { do_control_d(); });
            command_reader_.add_command(NEWLINE, [this] { do_accept_command(); });
            command_reader_.add_command(TAB, [this] { do_autocomplete(); });
            command_reader_.add_command(MOVE_LEFT, [this] { do_move_left(); });
            command_reader_.add_command(MOVE_RIGHT, [this] { do_move_right(); });
            command_reader_.add_command(MOVE_DOWN, [this] { do_history_up(); });
            command_reader_.add_command(MOVE_UP, [this] { do_history_down(); });
            command_reader_.set_default([this] { do_write_char(); });
        }

        Readline(const Readline &) = default;
        Readline(Readline &&) = default;

        ~Readline() { terminal_.reset_settings(); }

        std::string read(void) {

            buffer_.clear();
            command_reader_.start_reading();
            terminal_.move_cursor_horizontal_absolute();
            do_print_prompt();

            command_reader_.read_and_execute();

            return buffer_.data();
        }

        Readline &set_terminal_settings(const TerminalSettings &s) {
            settings_ = s;
            terminal_.set_settings(s);
            terminal_.apply_settings();
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
