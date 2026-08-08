#include <os64/cxx.hpp>

namespace {
struct Greeter {
    const char *message;
    Greeter() : message(nullptr) { message = "Global C++ constructors are working."; }
};
Greeter greeter;
}

int os64_main(const os64_api_t *api, const char *arguments) {
    os64::Console out(api);
    os64::unique_ptr<int> answer(new int(64));
    out << "OS64 native C++17 application\n" << greeter.message << '\n';
    out << "Arguments: " << arguments << '\n';
    return answer && *answer == 64 ? 0 : 2;
}
