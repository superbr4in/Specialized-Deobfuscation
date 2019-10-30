#pragma once

#include <revengine/expression_composition.hpp>

namespace rev
{
    struct instruction
    {
        struct address_order
        {
            using is_transparent = std::true_type;

            bool operator()(instruction const& instruction_1, instruction const& instruction_2) const;

            bool operator()(instruction const& instruction, std::uint64_t address) const;
            bool operator()(std::uint64_t address, instruction const& instruction) const;
        };

        std::uint64_t address;
        std::size_t size;

        expression_composition impact;
    };
}