#include "stdafx.h"

#include "../Bin-Capstone/capstone.h"

#include "disassembly.h"
#include "obfuscation.h"
#include "serialization.h"

#define FILE_1 "text1.dis"
#define FILE_2 "text2.dis"

static void process(const std::vector<uint8_t> bytes, const uint64_t base_address, const size_t length, const std::string out_file_name)
{
    const auto start = bytes.begin() + base_address;
    const std::vector<uint8_t> section(start, start + length);

    disassembly_x86::create_complete(base_address, section).save(out_file_name);
}

int main(const int argc, char* argv[])
{
    if (argc != 2)
    {
        std::cerr << "Invalid number of arguments." << std::endl;
        return -1;
    }

    const std::string file_name(argv[1]);

    std::cout << "File: \"" << file_name << "\"" << std::endl;

    std::vector<uint8_t> bytes;
    if (deserialize(file_name, bytes))
    {
        std::cerr << "Could not open file." << std::endl;
        return -1;
    }

    std::cout << "Size: " << bytes.size() << " bytes" << std::endl;

    // -----

/*
    process(bytes, 0x1000, 0x4b4a00, FILE_1);
    process(bytes, 0x989000, 0x4dd000, FILE_2);

    std::cout << "Complete" << std::endl;

    const auto seq = disassembly_x86::load(FILE_2)
        .find_sequences(10, X86_INS_PUSH, { X86_INS_PUSHFQ, X86_INS_MOVUPD, X86_INS_LEA });

    disassembly_x86::load(FILE_1).find_immediates(seq, { X86_INS_JMP, X86_INS_CALL });
*/

    const std::vector<disassembly_x86> disassemblies =
    {
        disassembly_x86::load(FILE_1),
        disassembly_x86::load(FILE_2)
    };

    std::cin.get();

    const auto obfuscations = obfuscation_framed_x86::pick_all(&disassemblies);

    // -----

    std::cin.get();
    return 0;
}
