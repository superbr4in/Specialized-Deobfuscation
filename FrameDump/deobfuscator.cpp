#include "stdafx.h"

#include "deobfuscator.h"

static bool verbose = false;

static std::vector<uint8_t> assemble_x86(const uint64_t address, const std::string string)
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

obfuscation_graph_x86::node::node(const std::shared_ptr<debugger> debugger, const uint64_t address, const std::vector<uint8_t> stop,
    std::map<uint64_t, node*>& node_map, memory_monitor& monitor, uint64_t& stop_address, node* previous)
    : previous(previous)
{
    node_map.emplace(address, this);

    if (verbose)
        std::cout << std::hex << std::uppercase << address << std::endl;

    debugger->jump_to(address);
    traceback = debugger->step_into();

    monitor.inspect_access(traceback);

    if (traceback.has_failed())
    {
        std::cout << "Error: " << std::hex << std::uppercase << address;

        if (previous != nullptr && previous->traceback.has_failed())
        {
            std::cout << " (Stop)" << std::endl;
            return;
        }

        std::cout << std::endl;
    }
    
    if (traceback->code == stop)
    {
        if (verbose)
            std::cout << "Stop" << std::endl;

        stop_address = traceback->address;
        return;
    }

    std::vector<uint64_t> next_addresses;
    stack_representation saved_stack { };
    if (traceback->type == instruction_type::jump && traceback->is_conditional)
    {
        if (verbose)
            std::cout << "Jump" << std::endl;

        next_addresses.push_back(address + traceback->code.size());
        next_addresses.push_back(traceback->operands.at(0).imm);

        saved_stack = debugger->get_stack();
    }
    else next_addresses.push_back(debugger->next_instruction().address);

    for (auto i = 0; i < next_addresses.size(); ++i)
    {
        const auto next_address = next_addresses.at(i);

        if (node_map.find(next_address) == node_map.end())
        {
            if (i > 0)
                debugger->set_stack(saved_stack);

            next.push_back(new node(debugger, next_address, stop,
                node_map, monitor, stop_address, this));
            continue;
        }

        if (verbose)
            std::cout << "Loop: " << std::hex << std::uppercase << next_address << std::endl;
        next.push_back(node_map.at(next_address));
    }
}

obfuscation_graph_x86::obfuscation_graph_x86(const std::shared_ptr<debugger> debugger, const uint64_t root_address)
    : root_address_(root_address)
{
    const auto width = sizeof(uint64_t) * 2;
    const std::string title = "Obfuscation ";

    std::cout << title << std::right << std::setw(width)
              << std::hex << std::uppercase << root_address << std::endl;

    const auto root_instruction = debugger->disassemble_at(root_address);
    if (root_instruction.str_mnemonic != "push")
    {
        std::cout << "Unexpected root" << std::endl;
        return;
    }

    const auto stack = debugger->get_stack();

    root_ = node(debugger, root_address, assemble_x86(0, "pop " + root_instruction.str_operands),
        node_map_, monitor_, stop_address_);

    debugger->set_stack(stack);

    std::cout << std::string(title.size(), ' ') << std::right << std::setw(width)
              << std::hex << std::uppercase << stop_address_
              << " (" << std::dec << node_map_.size() << ")" << std::endl;
}

traceback_x86 obfuscation_graph_x86::find_traceback(const uint64_t address) const
{
    return node_map_.at(address)->traceback;
}

deobfuscator_x86::deobfuscator_x86(loader& loader, const std::vector<uint8_t> code)
    : debugger_(std::make_shared<debugger>(loader, code)) { }

std::vector<obfuscation_graph_x86> deobfuscator_x86::inspect_framed(const std::vector<uint64_t> addresses) const
{
    std::vector<obfuscation_graph_x86> graphs;
    for (auto i = 1; i < 2; ++i)
    {
        std::cout << std::dec << i + 1 << ":" << std::endl;
        graphs.push_back(obfuscation_graph_x86(debugger_, addresses.at(i)));
        std::cout << std::endl;
    }

    return graphs;
}
