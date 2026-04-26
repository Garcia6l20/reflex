import reflex.cli;
import std;

using namespace reflex;

struct [[= cli::command{"hello"}]] hello_command {

    [[= cli::option{"-n/--name", "Your name"}]]
    std::string name = "World";

    void operator()() const {
        std::println("Hello, {}!", name);
    }
};

int main(int argc, const char* argv[]) {
    return cli::run<hello_command>(argc, argv);
}
