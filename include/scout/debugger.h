#pragma once

#include <istream>
#include <memory>
#include <set>
#include <vector>

#include "instruction.h"

#include "../../submodules/capstone/include/capstone.h"
#include "../../submodules/unicorn/include/unicorn/unicorn.h"

class debugger
{
    std::shared_ptr<csh> cs_;
    std::shared_ptr<uc_engine> uc_;

    int ip_register_;

public:

    debugger() = default;

    /**
     * Reads the instruction pointer value.
     * \returns The current emulated memory address.
     */
    uint64_t position() const;
    /**
     * Edits the instruction pointer value.
     * \param [in] address The desired emulated memory address.
     * \returns True if the new address is mapped in emulated memory, otherwise false.
     */
    bool position(uint64_t address) const;

    /**
     * Inquires the current instruction.
     * \returns The machine instruction the instruction pointer currently points to.
     */
    std::unique_ptr<machine_instruction> current_instruction() const;

    /**
     * Indicates whether the instruction pointer references mapped memory.
     * \return True if the pointed address is mapped in emulated memory, otherwise false.
     */
    bool is_mapped() const;
    /**
     * Indicates whether a memory address points to mapped memory.
     * \param [in] address The emulated memory address to be evaluated.
     * \returns True if the specified address is mapped in emulated memory, otherwise false.
     */
    bool is_mapped(uint64_t address) const;

    /**
     * Sets the instruction pointer to the next instruction without emulating the current one.
     * \returns True if the new address is mapped in emulated memory, otherwise false.
     */
    bool skip() const;
    /**
     * Advances the instruction pointer.
     * \param [in] count The number of bytes to be skipped.
     * \returns True if the new address is mapped in emulated memory, otherwise false.
     */
    bool skip(uint64_t count) const;

    /**
     * Emulates the current instruction.
     * \remarks Updates emulated registers and memory.
     * \returns True if the emulation was successful, otherwise false.
     */
    bool step_into() const;

    friend std::istream& operator>>(std::istream& is, debugger& debugger);

private:

    uint64_t read_register(int id) const;
    void write_register(int id, uint64_t value) const;

    void allocate_memory(uint64_t address, size_t size) const;
    void allocate_memory(uint64_t address, std::vector<uint8_t> const& data) const;

    std::vector<uint8_t> read_memory(uint64_t address, size_t size) const;
    void write_memory(uint64_t address, std::vector<uint8_t> const& data) const;

    std::set<uc_mem_region> get_memory_regions() const;

    cs_err get_cs_error() const;
    uc_err get_uc_error() const;
};