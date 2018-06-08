#include "stdafx.h"

#include "debugger.h"

debugger::debugger(loader& loader, const std::vector<uint8_t> code)
    : loader_(loader)
{
    const auto machine = loader_.load(code);

    disassembler_ = std::make_unique<disassembler>(machine);
    emulator_ = loader_.get_emulator();

    next_instruction_ = disassemble_at(emulator_->address());
}

std::shared_ptr<instruction> debugger::next_instruction() const
{
    return next_instruction_;
}

debug_trace_entry debugger::run(const size_t count)
{
    debug_trace_entry trace_entry;

    size_t i = 0;

    bool cont;
    do
    {
        trace_entry = step_into();

        cont = true;

        const auto address = next_instruction_->address;
        if (debug_points_.find(address) != debug_points_.end())
        {
            const auto dp = debug_points_.at(address);

            switch(dp)
            {
            case dp_break:
                cont = false;
                break;
            case dp_skip:
                skip();
                break;
            case dp_take:
                take();
                break;
            }
        }

        if (count > 0 && ++i >= count)
            break;
    }
    while (cont);

    return trace_entry;
}

debug_trace_entry debugger::step_into()
{
    debug_trace_entry trace_entry;

    trace_entry.address = next_instruction_->address;

    if (global_flags.hot)
        ++counter_[trace_entry.address];

    for (const auto reg : next_instruction_->registers)
        trace_entry.old_registers.emplace(reg.first, emulator_->reg_read<uint64_t>(reg.first));

    trace_entry.error = emulator_->emulate_once();
    if (trace_entry.error)
        trace_entry.error_str = uc_strerror(static_cast<uc_err>(trace_entry.error));

    switch (trace_entry.error)
    {
    case UC_ERR_READ_UNMAPPED:
    case UC_ERR_WRITE_UNMAPPED:
    case UC_ERR_FETCH_UNMAPPED:
        if (loader_.ensure_availability(emulator_->address()))
        {
            emulator_->jump_to(next_instruction_->address);
            return step_into(); // TODO: Prevent stack overflow
        }
    default:;
    }

    for (const auto reg : next_instruction_->registers)
        trace_entry.new_registers.emplace(reg.second, emulator_->reg_read<uint64_t>(reg.first));

    if (trace_entry.error && global_flags.ugly)
        skip();
    else next_instruction_ = disassemble_at(emulator_->address());

    trace_.push_back(std::make_unique<debug_trace_entry>(trace_entry));
    if (trace_.size() > MAX_TRACE)
        trace_.pop_front();

    return trace_entry;
}

int debugger::step_back()
{
    ERROR_IF(trace_.size() == 0);

    const auto& prev = trace_.at(trace_.size() - 1);

    ERROR_IF(jump_to(prev->address));

    for (const auto old_reg : prev->old_registers)
        emulator_->reg_write(old_reg.first, old_reg.second);

    trace_.pop_back();

    return RES_SUCCESS;
}

int debugger::set_debug_point(const uint64_t address, const debug_point point)
{
    ERROR_IF(!emulator_->mem_is_mapped(address));

    debug_points_.emplace(address, point);
    return RES_SUCCESS;
}
int debugger::remove_debug_point(const uint64_t address)
{
    ERROR_IF(!is_debug_point(address));

    debug_points_.erase(address);
    return RES_SUCCESS;
}

int debugger::jump_to(const uint64_t address)
{
    ERROR_IF(!emulator_->mem_is_mapped(address));

    emulator_->jump_to(address);
    next_instruction_ = disassemble_at(address);
    return RES_SUCCESS;
}
int debugger::get_raw(const uint64_t address, uint64_t& raw_address) const
{
    raw_address = loader_.to_raw_address(address);

    ERROR_IF(raw_address == -1);
    return RES_SUCCESS;
}

int debugger::skip()
{
    return jump_to(next_instruction_->address + next_instruction_->bytes.size());
}
int debugger::take()
{
    const auto jump = next_instruction_->jump;
    ERROR_IF(!jump.has_value());

    return jump_to(jump.value());
}

bool debugger::is_debug_point(const uint64_t address) const
{
    return debug_points_.find(address) != debug_points_.end();
}

std::shared_ptr<instruction> debugger::disassemble_at(const uint64_t address) const
{
    std::vector<uint8_t> bytes(MAX_BYTES);
    emulator_->mem_read(address, bytes);

    auto ins = disassembler_->disassemble(bytes, address);
    ins.label = loader_.label_at(address);

    return std::make_shared<instruction>(ins);
}
