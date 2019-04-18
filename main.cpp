#include <sys/mman.h>

#include <iostream>
#include <string.h>
#include <stdint.h>
#include <vector>
#include <algorithm>
#include <system_error>

typedef unsigned char byte;

namespace HQ9P
{
/* x86-64 assmebly, Linux write() syscall */
static const std::vector<byte> func_push = {
    0x55,            // push   %rbp
    0x48, 0x89, 0xe5 // mov    %rsp,%rbp
};

static const std::vector<byte> func_pop = {
    0x5d,            // pop    %rbp
    0xc3             // retq
};

static std::vector<byte> hello_world = {
    0xb8, 0x01, 0x00, 0x00, 0x00,             // mov    $0x1,%eax
    0xbf, 0x01, 0x00, 0x00, 0x00,             // mov    $0x1,%edi
    //          0xAB  0xAD  0xCA  0xFE
    0x48, 0xbe, 0x00, 0x00, 0x00, 0x00, 0x00, // movabs $0xABADCAFE,%rsi <- text address
    0x00, 0x00, 0x00,                         //
    0xba, 0x0e, 0x00, 0x00, 0x00,             // mov    $0xE,%edx <- E is output size (14)
    0x0f, 0x05                                // syscall
};

static std::vector<byte> quine = hello_world;

static std::vector<byte> increment = {
    0x48, 0xb8, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00,       // movabs $0xABADCAFE,%rax <- counter address
    0x48, 0x8b, 0x10,                         // mov    (%rax),%rdx
    0x48, 0x83, 0xc2, 0x01,                   // add    $0x1,%rdx
    0x48, 0x89, 0x10                          // mov    %rdx,(%rax)
};

const size_t text_address      = 12;
const size_t code_size_address  = 21;
const size_t counter_address        = 2;
}

void execute_stored_code(const std::vector<byte>& code)
{
    // Allocate
    void* memory = mmap(nullptr, code.size(), PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (memory == MAP_FAILED) {
        std::error_code ec(errno, std::system_category());
        throw std::system_error(ec, "Failed to allocate memory for the code");
    }

    // Copy
    std::copy(code.cbegin(), code.cend(), static_cast<byte*>(memory));

    // Protect (r-x)
    if (mprotect(memory, code.size(), PROT_READ | PROT_EXEC) != 0) {
        std::error_code ec(errno, std::system_category());
        throw std::system_error(ec, "Failed to change memory permissions");
    }

    // Execute
    (reinterpret_cast<void(*)(void)>(memory))();

    // Deallocate
    if (munmap(memory, code.size()) != 0) {
        std::cerr << "Warning: failed to deallocate code memory: "
                  << strerror(errno) << std::endl;
    }
}

int main()
{
    std::string input;
    std::cin >> input;
    input.append("\n");

    /* Patching the assmebly */
    // "H" command
    const char* hw = "Hello, world!\n";
    *(reinterpret_cast<const char**>(&HQ9P::hello_world[HQ9P::text_address])) = hw;
    // "Q" command
    const char* source = input.c_str();
    *(reinterpret_cast<const char**>(&HQ9P::quine[HQ9P::text_address])) = source;
    *(reinterpret_cast<uint32_t*>(&HQ9P::quine[HQ9P::code_size_address])) = static_cast<uint32_t>(strlen(source));
    // "+" command
    uint64_t counter = 0;
    *(reinterpret_cast<uint64_t**>(&HQ9P::increment[HQ9P::counter_address])) = &counter;

    /* Compiling the program */
    std::vector<byte> compiled(HQ9P::func_push.begin(), HQ9P::func_push.end());
    for (char cmd : input) {
        switch (cmd) {
        case 'H':
            compiled.insert(compiled.end(), HQ9P::hello_world.begin(), HQ9P::hello_world.end());
            break;
        case 'Q':
            compiled.insert(compiled.end(), HQ9P::quine.begin(), HQ9P::quine.end());
            break;
        case '9':
            std::cerr << "Error: this implementation of HQ9+ was created to promote a healthy lifestyle.\n"
                         "So, beer is unsupported and not recommended!" << std::endl;
            return EXIT_FAILURE;
        case '+':
            compiled.insert(compiled.end(), HQ9P::increment.begin(), HQ9P::increment.end());
            break;
        case '\n':
            continue;
        default:
            std::cerr << "Compilation error: unexpected symbol '" << cmd << "'" << std::endl;
            return EXIT_FAILURE;
        }
    }
    compiled.insert(compiled.end(), HQ9P::func_pop.begin(), HQ9P::func_pop.end());

    /* Executing */
    try {
        execute_stored_code(compiled);
    } catch (std::system_error& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
