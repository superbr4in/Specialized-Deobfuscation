#include "stdafx.h"

#include "debugger.h"

#include "instruction_32.h"
#include "register_state_32.h"

#define API extern "C" __declspec(dllexport)

API debugger* open_pe(
    const char* file_name)
{
    return new debugger(std::string(file_name));
}
API debugger* open_bytes(
    const char* bytes, const size_t size)
{
    return new debugger(std::vector<char>(bytes, bytes + size));
}

API void close(
    debugger* handle)
{
    handle->close();
    free(handle); // TODO: Necessary?
}

API void debug_32(
    debugger* handle,
    instruction_32& instruction)
{
    instruction = handle->debug_32();
}

API void get_register_state_32(
    debugger* handle,
    register_state_32& register_state)
{
    register_state = handle->get_registers_32();
}
