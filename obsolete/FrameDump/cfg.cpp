#include "stdafx.h"

#include "cfg.h"
#include "display.h"

#define CHAR_ID '*'
#define CHAR_PREV '#'
#define CHAR_NEXT '~'

static std::vector<uint8_t> assemble_x86(const uint64_t address, const std::string& string)
{
    const std::string var_code = "v1";
    const std::string var_length = "v2";

    auto s_python =
        "from keystone import *\n"
        "ks = Ks(KS_ARCH_X86, KS_MODE_64)\n"
        + var_code + ", count = ks.asm(b\"" + string + "\", " + std::to_string(address) + ")\n"
        + var_length + " = len(" + var_code + ")";

    Py_Initialize();
    PyRun_SimpleString(s_python.c_str());

    const auto main = PyImport_AddModule("__main__");

    const auto p_code = PyObject_GetAttrString(main, var_code.c_str());
    const auto length = _PyInt_AsInt(PyObject_GetAttrString(main, var_length.c_str()));

    std::vector<uint8_t> code;
    for (auto i = 0; i < length; ++i)
        code.push_back(_PyInt_AsInt(PyList_GetItem(p_code, i)));

    Py_Finalize();

    return code;
}
static void replace_first(std::string& string, const char old_char, const char new_char)
{
    const auto pos = string.find_first_of(old_char);

    if (pos == std::string::npos)
        return;

    string = string.substr(0, pos) + new_char + string.substr(pos + 1);
}

std::string cfg_x86::block::to_string() const
{
    const std::string l = "| ";
    const std::string r = " |";

    const auto h = '-';

    const auto eu = '.';
    const auto ed = '\'';

    std::ostringstream ss;

    const auto last = trace.back();
    const auto last_string = last.instruction.to_string(last.instruction.is_conditional || last.instruction.is_volatile);

    const auto width = last_string.size();

    const auto padding = 1;

    ss << std::string(padding, ' ') << CHAR_ID << std::setfill(h) << std::setw(width + l.size() + r.size() - 2) << std::left << "(" + std::to_string(trace.size()) + ")" << eu;
    for (unsigned i = 0; i < previous.size(); ++i)
        ss << ' ' << CHAR_PREV;
    ss << std::endl;

    ss << std::setfill(' ');

    if (trace.size() > 1)
        ss << std::string(padding, ' ') << l << std::setw(width) << std::left << trace.front().instruction.to_string(false) << r << std::endl;
    if (trace.size() > 2)
        ss << std::string(padding, ' ') << l << std::setw(width) << std::left << ':' << r << std::endl;
    ss << std::string(padding, ' ') << l << std::setw(width) << std::left << last_string << r << std::endl;

    ss << std::string(padding, ' ') << ed << std::string(width + 2, '-') << ed;
    for (unsigned i = 0; i < next.size(); ++i)
        ss << ' ' << CHAR_NEXT;

    return ss.str();
}

cfg_x86::cfg_x86(const std::shared_ptr<debugger>& debugger, const uint64_t root_address)
{
    const auto root_instruction = debugger->disassemble_at(root_address);
    if (root_instruction.str_mnemonic != "push")
    {
        std::cout << "Unexpected root" << std::endl;
        return;
    }

    std::map<block*, block*> redir;
    paths_ = enumerate_paths(
        build(debugger, root_address, assemble_x86(0, "pop " + root_instruction.str_operands), map_, redir));
}

void cfg_x86::draw() const
{
    std::map<block, char> map1;
    std::map<char, block> map2;

    auto id = 'A';
    for (const auto m : map_)
    {
        const auto [it, b] = map1.try_emplace(*m.second.first, id);
        if (b)
            map2.emplace(id++, *m.second.first);
    }

    for (const auto& [block_id, block] : map2)
    {
        const auto no_pred = block.previous.empty();
        const auto no_succ = block.next.empty();

        if (no_pred ^ no_succ)
        {
            std::cout << dsp::colorize(FOREGROUND_INTENSITY |
                (no_pred ? FOREGROUND_GREEN : FOREGROUND_RED));
        }

        auto block_string = block.to_string();

        replace_first(block_string, CHAR_ID, block_id);

        for (const auto p : block.previous)
            replace_first(block_string, CHAR_PREV, map1.at(*p));
        for (const auto [c, n] : block.next)
            replace_first(block_string, CHAR_NEXT, map1.at(*n));

        std::cout << block_string << std::endl << std::endl << dsp::decolorize;
    }

    std::cout << "Found " << paths_.size() << " path";
    if (paths_.size() > 1)
        std::cout << 's';
    std::cout << " without cycles:" << std::endl;
    for (const auto& p : paths_)
    {
        for (unsigned i = 0; i < p.blocks.size(); ++i)
        {
            if (i > 0)
                std::cout << '-';
            std::cout << map1.at(*p.blocks.at(i));
        }

        std::cout << std::endl;
    }
}

cfg_x86::block* cfg_x86::build(const std::shared_ptr<debugger>& debugger, uint64_t address, const std::vector<uint8_t>& stop,
    std::map<uint64_t, std::pair<block*, size_t>>& map, std::map<block*, block*>& redir)
{
    // New (current) block
    const auto cur = new block;

    // Appends an existing block at the specified address as successor
    const std::function<bool(uint64_t, std::optional<x86_insn>)> success = [cur, &map, &redir](const uint64_t next_address, const std::optional<x86_insn> condition)
    {
        const auto map_it = map.find(next_address);
        if (map_it == map.end())
        {
            // No block exists at this address
            return false;
        }

        const auto [orig, index] = map_it->second;

        if (index == 0)
        {
            // Block does not have to be split
            orig->previous.insert(cur);
            cur->next.emplace_back(condition, orig);
            return true;
        }

        const auto begin = orig->trace.begin() + index;
        const auto end = orig->trace.end();

        const auto next = new block;

        // Copy tail
        next->trace = std::vector<traceback_x86>(begin, end);

        // Update map
        // TODO: Inefficient with large blocks
        for (auto j = 0; j < end - begin; ++j)
            map[(begin + j)->instruction.address] = std::make_pair(next, j);

        // Truncate tail
        orig->trace.erase(begin, end);

        // Update successor information
        cur->next.emplace_back(condition, next);
        next->next = orig->next;
        orig->next.clear();
        orig->next.emplace_back(std::nullopt, next);

        // Update predecessor information
        next->previous.insert(orig);
        for (const auto [c, nn] : next->next)
        {
            nn->previous.erase(orig);
            nn->previous.insert(next);
        }
        next->previous.insert(cur);

        // Redirect later successor declarations
        redir[orig] = next;

        return true;
    };

    // Repeat until successors are set
    while (cur->next.empty())
    {
        // Map address to block and index
        map.emplace(address, std::make_pair(cur, cur->trace.size()));

        // Emulate instruction and apped traceback
        debugger->jump_to(address);
        const auto instruction = debugger->next_instruction();
        cur->trace.emplace_back(instruction, debugger->step_into(), debugger->get_context());

        if (instruction.code == stop)
        {
            // Reached final instruction, stop without successor
            break;
        }

        if (instruction.type == ins_jump && instruction.is_conditional)
        {
            std::vector<uint64_t> next_addresses;

            // Consider both jump results
            next_addresses.push_back(address + instruction.code.size());
            next_addresses.push_back(std::get<op_immediate>(instruction.operands.at(0).value));

            // Save current emulation state
            const auto context = debugger->get_context();

            // Reset a prior redirection
            redir[cur] = cur;

            for (const auto next_address : next_addresses)
            {
                if (!success(next_address, instruction.id))
                {
                    // Recursively create a new successor
                    const auto next = build(debugger, next_address, stop, map, redir);

                    // React to eventual redirections
                    const auto r = redir[cur];
                    next->previous.insert(r);
                    r->next.emplace_back(instruction.id, next);
                }

                // Reset to original state
                debugger->set_context(context);
            }
        }
/*
        else if (instruction.is_volatile) // TODO
        {
            const auto next_address = debugger->next_instruction().address;

            if (!success(next_address))
            {
                const auto next = build(debugger, next_address, stop, map);
                next->previous.insert(cur);
                cur->next.insert(next);
            }
        }
*/
        else
        {
            const auto next_address = debugger->next_instruction().address;

            if (!success(next_address, std::nullopt))
            {
                // Advanced address and continue
                address = next_address;
            }
        }
    }

    return cur;
}

std::vector<cfg_x86::path> cfg_x86::enumerate_paths(block* const root, std::map<block*, bool> map,
    std::vector<x86_insn> conditions, std::vector<block*> passed)
{
    if (map[root])
        return { };

    passed.push_back(root);

    if (root->next.empty())
        return { path { conditions, passed } };

    std::vector<path> paths;
    map[root] = true;

    for (const auto [c, n] : root->next)
    {
        if (c.has_value())
            conditions.push_back(c.value());
        const auto next = enumerate_paths(n, map, conditions, passed);
        paths.insert(paths.end(), next.begin(), next.end());
    }

    map[root] = false;
    return paths;
}

bool operator<(const cfg_x86::block& block1, const cfg_x86::block& block2)
{
    return block1.trace.front().instruction.address < block2.trace.front().instruction.address;
}