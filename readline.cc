#include <iostream>
#include <string>

class Readline {};

#include <termios.h>
#include <unistd.h>
#include <system_error>

struct EscapeSequence {
    static const std::string ClearTheScreen;
};

using namespace std::literals;

const std::string EscapeSequence::ClearTheScreen{"\x1b[2J"s};

class Terminal {
    private:
        termios original_ = get_terminal_attr();
        termios current_ = get_terminal_attr();

        static termios get_terminal_attr(void) {
            termios term;

            int rv = tcgetattr(STDIN_FILENO, &term);

            if (rv)
                throw std::system_error{errno, std::generic_category()};

            return term;
        }

        static void set_terminal_attr(const termios term) {
            int rv = tcsetattr(STDIN_FILENO, TCSANOW, &term);

            if (rv)
                throw std::system_error{errno, std::generic_category()};
        }

    public:
        ~Terminal() {
            // NOTE: this will abort program if an exception occured
            set_terminal_attr(original_);
        }

        void set_echo(bool to) {
            current_.c_lflag &= to ? ECHO : ~ECHO;
            set_terminal_attr(current_);
        }

        void set_canonical(bool to) {
            current_.c_lflag &= to ? ICANON : ~ICANON;
            set_terminal_attr(current_);
        }

        void set_min_chars_for_canonical_read(size_t n) {
            current_.c_cc[VMIN] = n;
            set_terminal_attr(current_);
        }

        void set_timeout_for_non_canonical_read(size_t n) {
            current_.c_cc[VTIME] = n;
            set_terminal_attr(current_);
        }

        void set_ctrlc_ctrlz_as_characters(bool to) {
            current_.c_lflag &= to ? ~ISIG : ISIG;
            set_terminal_attr(current_);

        }

        static Terminal raw() {
            Terminal term;

            term.set_echo(false);
            term.set_canonical(false);
            term.set_ctrlc_ctrlz_as_characters(true);

            term.set_min_chars_for_canonical_read(1);
            term.set_timeout_for_non_canonical_read(0);

            return term;
        }

        void cursor_forward() const {}
        void cursor_backward() const {}
        void clear_the_screen() const {
            write(STDOUT_FILENO, EscapeSequence::ClearTheScreen.c_str(), 4);
        }

};


char constexpr ctrl_key(char c) {
    return c & 0x1F;
}


int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    Terminal term{Terminal::raw()};
    while (char t = std::cin.get()) {

        switch (t) {
            case ctrl_key(3):
                goto done;
            case ctrl_key(4):
                term.clear_the_screen();
            default:
                std::cout << int{t} << std::flush;
        }
    }
done:
    ;
}
