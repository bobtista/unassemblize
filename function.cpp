/**
 * @file
 *
 * @brief Class encapsulating a single function disassembly.
 *
 * @copyright Unassemblize is free software: you can redistribute it and/or
 *            modify it under the terms of the GNU General Public License
 *            as published by the Free Software Foundation, either version
 *            3 of the License, or (at your option) any later version.
 *            A full copy of the GNU General Public License can be found in
 *            LICENSE
 */
#include "function.h"
#include <Zycore/Format.h>
#include <Zydis/Zydis.h>
#include <inttypes.h>
#include <iomanip>
#include <sstream>
#include <string.h>

namespace unassemblize
{
namespace
{
uint32_t get_le32(const uint8_t *data)
{
    return (data[3] << 24) | (data[2] << 16) | (data[1] << 8) | data[0];
}

// Copy of the disassemble function without any formatting.
static ZyanStatus UnasmDisassembleNoFormat(ZydisMachineMode machine_mode, ZyanU64 runtime_address, const void *buffer,
    ZyanUSize length, ZydisDisassembledInstruction *instruction)
{
    if (!buffer || !instruction) {
        return ZYAN_STATUS_INVALID_ARGUMENT;
    }

    memset(instruction, 0, sizeof(*instruction));
    instruction->runtime_address = runtime_address;

    // Derive the stack width from the address width.
    ZydisStackWidth stack_width;
    switch (machine_mode) {
        case ZYDIS_MACHINE_MODE_LONG_64:
            stack_width = ZYDIS_STACK_WIDTH_64;
            break;
        case ZYDIS_MACHINE_MODE_LONG_COMPAT_32:
        case ZYDIS_MACHINE_MODE_LEGACY_32:
            stack_width = ZYDIS_STACK_WIDTH_32;
            break;
        case ZYDIS_MACHINE_MODE_LONG_COMPAT_16:
        case ZYDIS_MACHINE_MODE_LEGACY_16:
        case ZYDIS_MACHINE_MODE_REAL_16:
            stack_width = ZYDIS_STACK_WIDTH_16;
            break;
        default:
            return ZYAN_STATUS_INVALID_ARGUMENT;
    }

    ZydisDecoder decoder;
    ZYAN_CHECK(ZydisDecoderInit(&decoder, machine_mode, stack_width));

    ZydisDecoderContext ctx;
    ZYAN_CHECK(ZydisDecoderDecodeInstruction(&decoder, &ctx, buffer, length, &instruction->info));
    ZYAN_CHECK(ZydisDecoderDecodeOperands(
        &decoder, &ctx, &instruction->info, instruction->operands, instruction->info.operand_count));

    return ZYAN_STATUS_SUCCESS;
}

ZydisFormatterFunc default_print_address_absolute;

static ZyanStatus UnasmFormatterPrintAddressAbsolute(
    const ZydisFormatter *formatter, ZydisFormatterBuffer *buffer, ZydisFormatterContext *context)
{
    Function *func = static_cast<Function *>(context->user_data);
    uint64_t address;
    ZYAN_CHECK(ZydisCalcAbsoluteAddress(context->instruction, context->operand, context->runtime_address, &address));
    char hex_buff[32];
    const Executable::Symbol &symbol = func->executable().get_symbol(address);

    if (!symbol.name.empty()) {
        ZYAN_CHECK(ZydisFormatterBufferAppend(buffer, ZYDIS_TOKEN_SYMBOL));
        ZyanString *string;
        ZYAN_CHECK(ZydisFormatterBufferGetString(buffer, &string));
        auto it = func->labels().find(address);
        return ZyanStringAppendFormat(string, "%s", symbol.name.c_str());
    } else if (address >= func->section_address() && address < func->section_end()) {
        // Probably a function if the address is in the current section.
        ZYAN_CHECK(ZydisFormatterBufferAppend(buffer, ZYDIS_TOKEN_SYMBOL));
        ZyanString *string;
        ZYAN_CHECK(ZydisFormatterBufferGetString(buffer, &string));
        const Executable::Symbol &symbol = func->executable().get_symbol(address);

        if (!symbol.name.empty()) {
            func->add_dependency(symbol.name);
            return ZyanStringAppendFormat(string, "%s", symbol.name.c_str());
        }

        snprintf(hex_buff, sizeof(hex_buff), "sub_%" PRIx64, address);
        func->add_dependency(hex_buff);

        return ZyanStringAppendFormat(string, "sub_%" PRIx64, address);
    } else if (address >= func->executable().base_address() && address < func->executable().end_address()) {
        // Data is in another section?
        ZYAN_CHECK(ZydisFormatterBufferAppend(buffer, ZYDIS_TOKEN_SYMBOL));
        ZyanString *string;
        ZYAN_CHECK(ZydisFormatterBufferGetString(buffer, &string));
        const Executable::Symbol &symbol = func->executable().get_symbol(address);

        if (!symbol.name.empty()) {
            func->add_dependency(symbol.name);
            return ZyanStringAppendFormat(string, "%s", symbol.name.c_str());
        }

        snprintf(hex_buff, sizeof(hex_buff), "off_%" PRIx64, address);
        func->add_dependency(hex_buff);

        return ZyanStringAppendFormat(string, "off_%" PRIx64, address);
    }

    return default_print_address_absolute(formatter, buffer, context);
}

ZydisFormatterFunc default_print_address_relative;

static ZyanStatus UnasmFormatterPrintAddressRelative(
    const ZydisFormatter *formatter, ZydisFormatterBuffer *buffer, ZydisFormatterContext *context)
{
    Function *func = static_cast<Function *>(context->user_data);
    uint64_t address;
    ZYAN_CHECK(ZydisCalcAbsoluteAddress(context->instruction, context->operand, context->runtime_address, &address));
    char hex_buff[32];
    const Executable::Symbol &symbol = func->executable().get_symbol(address);

    if (!symbol.name.empty()) {
        ZYAN_CHECK(ZydisFormatterBufferAppend(buffer, ZYDIS_TOKEN_SYMBOL));
        ZyanString *string;
        ZYAN_CHECK(ZydisFormatterBufferGetString(buffer, &string));
        auto it = func->labels().find(address);
        return ZyanStringAppendFormat(string, "%s", symbol.name.c_str());
    } else if (address >= func->section_address() && address < func->section_end()) {
        // Probably a function if the address is in the current section.
        ZYAN_CHECK(ZydisFormatterBufferAppend(buffer, ZYDIS_TOKEN_SYMBOL));
        ZyanString *string;
        ZYAN_CHECK(ZydisFormatterBufferGetString(buffer, &string));
        const Executable::Symbol &symbol = func->executable().get_symbol(address);

        if (!symbol.name.empty()) {
            func->add_dependency(symbol.name);
            return ZyanStringAppendFormat(string, "%s", symbol.name.c_str());
        }

        snprintf(hex_buff, sizeof(hex_buff), "sub_%" PRIx64, address);
        func->add_dependency(hex_buff);

        return ZyanStringAppendFormat(string, "sub_%" PRIx64, address);
    } else if (address >= func->executable().base_address() && address < func->executable().end_address()) {
        // Data if in another section?
        ZYAN_CHECK(ZydisFormatterBufferAppend(buffer, ZYDIS_TOKEN_SYMBOL));
        ZyanString *string;
        ZYAN_CHECK(ZydisFormatterBufferGetString(buffer, &string));
        const Executable::Symbol &symbol = func->executable().get_symbol(address);

        if (!symbol.name.empty()) {
            func->add_dependency(symbol.name);
            return ZyanStringAppendFormat(string, "%s", symbol.name.c_str());
        }

        snprintf(hex_buff, sizeof(hex_buff), "off_%" PRIx64, address);
        func->add_dependency(hex_buff);

        return ZyanStringAppendFormat(string, "off_%" PRIx64, address);
    }

    return default_print_address_relative(formatter, buffer, context);
}

ZydisFormatterFunc default_print_immediate;

static ZyanStatus UnasmFormatterPrintIMM(
    const ZydisFormatter *formatter, ZydisFormatterBuffer *buffer, ZydisFormatterContext *context)
{
    Function *func = static_cast<Function *>(context->user_data);
    uint64_t address = context->operand->imm.value.u;
    char hex_buff[32];
    const Executable::Symbol &symbol = func->executable().get_symbol(address);

    if (!symbol.name.empty()) {
        ZYAN_CHECK(ZydisFormatterBufferAppend(buffer, ZYDIS_TOKEN_SYMBOL));
        ZyanString *string;
        ZYAN_CHECK(ZydisFormatterBufferGetString(buffer, &string));
        auto it = func->labels().find(address);
        return ZyanStringAppendFormat(string, "offset %s", symbol.name.c_str());
    } else if (address >= func->section_address() && address < func->section_end()) {
        // Probably a function if the address is in the current section.
        ZYAN_CHECK(ZydisFormatterBufferAppend(buffer, ZYDIS_TOKEN_SYMBOL));
        ZyanString *string;
        ZYAN_CHECK(ZydisFormatterBufferGetString(buffer, &string));
        const Executable::Symbol &symbol = func->executable().get_symbol(address);

        if (!symbol.name.empty()) {
            func->add_dependency(symbol.name);
            return ZyanStringAppendFormat(string, "offset %s", symbol.name.c_str());
        }

        snprintf(hex_buff, sizeof(hex_buff), "offset sub_%" PRIx64, address);
        func->add_dependency(hex_buff);

        return ZyanStringAppendFormat(string, "offset sub_%" PRIx64, address);
    } else if (address >= func->executable().base_address() && address < (func->executable().end_address())) {
        // Data if in another section?
        ZYAN_CHECK(ZydisFormatterBufferAppend(buffer, ZYDIS_TOKEN_SYMBOL));
        ZyanString *string;
        ZYAN_CHECK(ZydisFormatterBufferGetString(buffer, &string));
        const Executable::Symbol &symbol = func->executable().get_symbol(address);

        if (!symbol.name.empty()) {
            func->add_dependency(symbol.name);
            return ZyanStringAppendFormat(string, "offset %s", symbol.name.c_str());
        }

        snprintf(hex_buff, sizeof(hex_buff), "offset off_%" PRIx64, address);
        func->add_dependency(hex_buff);

        return ZyanStringAppendFormat(string, "offset off_%" PRIx64, address);
    }

    return default_print_immediate(formatter, buffer, context);
}

ZydisFormatterFunc default_print_displacement;

static ZyanStatus UnasmFormatterPrintDISP(
    const ZydisFormatter *formatter, ZydisFormatterBuffer *buffer, ZydisFormatterContext *context)
{
    Function *func = static_cast<Function *>(context->user_data);
    uint64_t address = context->operand->mem.disp.value;
    char hex_buff[32];
    const Executable::Symbol &symbol = func->executable().get_symbol(address);

    if (!symbol.name.empty()) {
        ZYAN_CHECK(ZydisFormatterBufferAppend(buffer, ZYDIS_TOKEN_SYMBOL));
        ZyanString *string;
        ZYAN_CHECK(ZydisFormatterBufferGetString(buffer, &string));
        auto it = func->labels().find(address);
        return ZyanStringAppendFormat(string, "+%s", symbol.name.c_str());
    } else if (address >= func->section_address() && address < func->section_end()) {
        // Probably a function if the address is in the current section.
        ZYAN_CHECK(ZydisFormatterBufferAppend(buffer, ZYDIS_TOKEN_SYMBOL));
        ZyanString *string;
        ZYAN_CHECK(ZydisFormatterBufferGetString(buffer, &string));
        const Executable::Symbol &symbol = func->executable().get_nearest_symbol(address);

        if (!symbol.name.empty()) {
            func->add_dependency(symbol.name);

            if (symbol.address == address) {
                return ZyanStringAppendFormat(string, "+%s", symbol.name.c_str());
            } else {
                uint64_t diff = address - symbol.address; // value should always be lower than requested address.
                return ZyanStringAppendFormat(string, "+%s+0x%" PRIx64, symbol.name.c_str(), diff);
            }
        }

        snprintf(hex_buff, sizeof(hex_buff), "sub_%" PRIx64, address);
        func->add_dependency(hex_buff);

        return ZyanStringAppendFormat(string, "+sub_%" PRIx64, address);
    } else if (address >= func->executable().base_address() && address < (func->executable().end_address())) {
        // Data if in another section?
        ZYAN_CHECK(ZydisFormatterBufferAppend(buffer, ZYDIS_TOKEN_SYMBOL));
        ZyanString *string;
        ZYAN_CHECK(ZydisFormatterBufferGetString(buffer, &string));
        const Executable::Symbol &symbol = func->executable().get_nearest_symbol(address);

        if (!symbol.name.empty()) {
            func->add_dependency(symbol.name);

            if (symbol.address == address) {
                return ZyanStringAppendFormat(string, "+%s", symbol.name.c_str());
            } else {
                uint64_t diff = address - symbol.address; // value should always be lower than requested address.
                return ZyanStringAppendFormat(string, "+%s+0x%" PRIx64, symbol.name.c_str(), diff);
            }
        }

        snprintf(hex_buff, sizeof(hex_buff), "off_%" PRIx64, address);
        func->add_dependency(hex_buff);

        return ZyanStringAppendFormat(string, "+off_%" PRIx64, address);
    }

    return default_print_displacement(formatter, buffer, context);
}

ZydisFormatterFunc default_format_operand_ptr;

static ZyanStatus UnasmFormatterFormatOperandPTR(
    const ZydisFormatter *formatter, ZydisFormatterBuffer *buffer, ZydisFormatterContext *context)
{
    Function *func = static_cast<Function *>(context->user_data);
    uint64_t address = context->operand->ptr.offset;
    char hex_buff[32];
    const Executable::Symbol &symbol = func->executable().get_symbol(address);

    if (!symbol.name.empty()) {
        ZYAN_CHECK(ZydisFormatterBufferAppend(buffer, ZYDIS_TOKEN_SYMBOL));
        ZyanString *string;
        ZYAN_CHECK(ZydisFormatterBufferGetString(buffer, &string));
        auto it = func->labels().find(address);
        return ZyanStringAppendFormat(string, "%s", symbol.name.c_str());
    } else if (address >= func->section_address() && address < func->section_end()) {
        // Probably a function if the address is in the current section.
        ZYAN_CHECK(ZydisFormatterBufferAppend(buffer, ZYDIS_TOKEN_SYMBOL));
        ZyanString *string;
        ZYAN_CHECK(ZydisFormatterBufferGetString(buffer, &string));
        const Executable::Symbol &symbol = func->executable().get_symbol(address);

        if (!symbol.name.empty()) {
            func->add_dependency(symbol.name);
            return ZyanStringAppendFormat(string, "%s", symbol.name.c_str());
        }

        snprintf(hex_buff, sizeof(hex_buff), "sub_%" PRIx64, address);
        func->add_dependency(hex_buff);

        return ZyanStringAppendFormat(string, "sub_%" PRIx64, address);
    } else if (address >= func->executable().base_address() && address < func->executable().end_address()) {
        // Data if in another section?
        ZYAN_CHECK(ZydisFormatterBufferAppend(buffer, ZYDIS_TOKEN_SYMBOL));
        ZyanString *string;
        ZYAN_CHECK(ZydisFormatterBufferGetString(buffer, &string));
        const Executable::Symbol &symbol = func->executable().get_symbol(address);

        if (!symbol.name.empty()) {
            func->add_dependency(symbol.name);
            return ZyanStringAppendFormat(string, "%s", symbol.name.c_str());
        }

        snprintf(hex_buff, sizeof(hex_buff), "unk_%" PRIx64, address);
        func->add_dependency(hex_buff);

        return ZyanStringAppendFormat(string, "unk_%" PRIx64, address);
    }

    return default_format_operand_ptr(formatter, buffer, context);
}

ZydisFormatterFunc default_format_operand_mem;

static ZyanStatus UnasmFormatterFormatOperandMEM(
    const ZydisFormatter *formatter, ZydisFormatterBuffer *buffer, ZydisFormatterContext *context)
{
    Function *func = static_cast<Function *>(context->user_data);
    uint64_t address = context->operand->mem.disp.value;
    char hex_buff[32];
    const Executable::Symbol &symbol = func->executable().get_symbol(address);

    if ((context->operand->mem.type == ZYDIS_MEMOP_TYPE_MEM) || (context->operand->mem.type == ZYDIS_MEMOP_TYPE_VSIB)) {
        ZYAN_CHECK(formatter->func_print_typecast(formatter, buffer, context));
    }
    ZYAN_CHECK(formatter->func_print_segment(formatter, buffer, context));

    if (!symbol.name.empty()) {
        ZYAN_CHECK(ZydisFormatterBufferAppend(buffer, ZYDIS_TOKEN_SYMBOL));
        ZyanString *string;
        ZYAN_CHECK(ZydisFormatterBufferGetString(buffer, &string));
        auto it = func->labels().find(address);
        return ZyanStringAppendFormat(string, "[%s]", symbol.name.c_str());
    } else if (address >= func->section_address() && address < func->section_end()) {
        // Probably a function if the address is in the current section.
        ZYAN_CHECK(ZydisFormatterBufferAppend(buffer, ZYDIS_TOKEN_SYMBOL));
        ZyanString *string;
        ZYAN_CHECK(ZydisFormatterBufferGetString(buffer, &string));
        const Executable::Symbol &symbol = func->executable().get_symbol(address);

        if (!symbol.name.empty()) {
            func->add_dependency(symbol.name);
            return ZyanStringAppendFormat(string, "[%s]", symbol.name.c_str());
        }

        snprintf(hex_buff, sizeof(hex_buff), "sub_%" PRIx64, address);
        func->add_dependency(hex_buff);

        return ZyanStringAppendFormat(string, "[sub_%" PRIx64 "]", address);
    } else if (address >= func->executable().base_address() && address < func->executable().end_address()) {
        // Data if in another section?
        ZYAN_CHECK(ZydisFormatterBufferAppend(buffer, ZYDIS_TOKEN_SYMBOL));
        ZyanString *string;
        ZYAN_CHECK(ZydisFormatterBufferGetString(buffer, &string));
        const Executable::Symbol &symbol = func->executable().get_symbol(address);

        if (!symbol.name.empty()) {
            func->add_dependency(symbol.name);
            return ZyanStringAppendFormat(string, "[%s]", symbol.name.c_str());
        }

        snprintf(hex_buff, sizeof(hex_buff), "unk_%" PRIx64, address);
        func->add_dependency(hex_buff);

        return ZyanStringAppendFormat(string, "[unk_%" PRIx64 "]", address);
    }

    return default_format_operand_mem(formatter, buffer, context);
}

ZydisFormatterRegisterFunc default_format_print_reg;

static ZyanStatus UnasmFormatterFormatPrintRegister(
    const ZydisFormatter *formatter, ZydisFormatterBuffer *buffer, ZydisFormatterContext *context, ZydisRegister reg)
{
    // Copied from internal FormatterBase.h
#define ZYDIS_BUFFER_APPEND_TOKEN(buffer, type) \
    if ((buffer)->is_token_list) { \
        ZYAN_CHECK(ZydisFormatterBufferAppend(buffer, type)); \
    }

    if (reg >= ZYDIS_REGISTER_ST0 && reg <= ZYDIS_REGISTER_ST7) {
        ZYDIS_BUFFER_APPEND_TOKEN(buffer, ZYDIS_TOKEN_REGISTER);
        ZyanString *string;
        ZYAN_CHECK(ZydisFormatterBufferGetString(buffer, &string));
        return ZyanStringAppendFormat(string, "st(%d)", reg - 69);
    }

    return default_format_print_reg(formatter, buffer, context, reg);
}

static ZyanStatus UnasmDisassembleCustom(ZydisMachineMode machine_mode, ZyanU64 runtime_address, const void *buffer,
    ZyanUSize length, ZydisDisassembledInstruction *instruction, void *user_data, ZydisFormatterStyle style)
{
    if (!buffer || !instruction) {
        return ZYAN_STATUS_INVALID_ARGUMENT;
    }

    memset(instruction, 0, sizeof(*instruction));
    instruction->runtime_address = runtime_address;

    // Derive the stack width from the address width.
    ZydisStackWidth stack_width;
    switch (machine_mode) {
        case ZYDIS_MACHINE_MODE_LONG_64:
            stack_width = ZYDIS_STACK_WIDTH_64;
            break;
        case ZYDIS_MACHINE_MODE_LONG_COMPAT_32:
        case ZYDIS_MACHINE_MODE_LEGACY_32:
            stack_width = ZYDIS_STACK_WIDTH_32;
            break;
        case ZYDIS_MACHINE_MODE_LONG_COMPAT_16:
        case ZYDIS_MACHINE_MODE_LEGACY_16:
        case ZYDIS_MACHINE_MODE_REAL_16:
            stack_width = ZYDIS_STACK_WIDTH_16;
            break;
        default:
            return ZYAN_STATUS_INVALID_ARGUMENT;
    }

    ZydisDecoder decoder;
    ZYAN_CHECK(ZydisDecoderInit(&decoder, machine_mode, stack_width));

    ZydisDecoderContext ctx;
    ZYAN_CHECK(ZydisDecoderDecodeInstruction(&decoder, &ctx, buffer, length, &instruction->info));
    ZYAN_CHECK(ZydisDecoderDecodeOperands(
        &decoder, &ctx, &instruction->info, instruction->operands, instruction->info.operand_count));

    ZydisFormatter formatter;
    ZYAN_CHECK(ZydisFormatterInit(&formatter, style));

    ZydisFormatterSetProperty(&formatter, ZYDIS_FORMATTER_PROP_FORCE_SIZE, ZYAN_TRUE);

    default_print_address_absolute = (ZydisFormatterFunc)&UnasmFormatterPrintAddressAbsolute;
    ZydisFormatterSetHook(
        &formatter, ZYDIS_FORMATTER_FUNC_PRINT_ADDRESS_ABS, (const void **)&default_print_address_absolute);

    default_print_immediate = (ZydisFormatterFunc)&UnasmFormatterPrintIMM;
    ZydisFormatterSetHook(&formatter, ZYDIS_FORMATTER_FUNC_PRINT_IMM, (const void **)&default_print_immediate);

    default_print_address_relative = (ZydisFormatterFunc)&UnasmFormatterPrintAddressRelative;
    ZydisFormatterSetHook(
        &formatter, ZYDIS_FORMATTER_FUNC_PRINT_ADDRESS_REL, (const void **)&default_print_address_relative);

    default_print_displacement = (ZydisFormatterFunc)&UnasmFormatterPrintDISP;
    ZydisFormatterSetHook(&formatter, ZYDIS_FORMATTER_FUNC_PRINT_DISP, (const void **)&default_print_displacement);

    default_format_operand_ptr = (ZydisFormatterFunc)&UnasmFormatterFormatOperandPTR;
    ZydisFormatterSetHook(&formatter, ZYDIS_FORMATTER_FUNC_FORMAT_OPERAND_PTR, (const void **)&default_format_operand_ptr);

    default_format_operand_mem = (ZydisFormatterFunc)&UnasmFormatterFormatOperandMEM;
    ZydisFormatterSetHook(&formatter, ZYDIS_FORMATTER_FUNC_FORMAT_OPERAND_MEM, (const void **)&default_format_operand_mem);

    default_format_print_reg = (ZydisFormatterRegisterFunc)&UnasmFormatterFormatPrintRegister;
    ZydisFormatterSetHook(&formatter, ZYDIS_FORMATTER_FUNC_PRINT_REGISTER, (const void **)&default_format_print_reg);

    ZYAN_CHECK(ZydisFormatterFormatInstruction(&formatter,
        &instruction->info,
        instruction->operands,
        instruction->info.operand_count_visible,
        instruction->text,
        sizeof(instruction->text),
        runtime_address,
        user_data));

    return ZYAN_STATUS_SUCCESS;
}

std::string BuildInvalidInstructionString(
    const ZydisDisassembledInstruction &ins, const uint8_t *ins_data, uint64_t runtime_address, uint64_t image_base)
{
    std::stringstream stream;
    stream.fill('0');
    stream << "; Unrecognized opcode at runtime-address:0x" << std::setw(8) << std::hex << runtime_address
           << " image-address:0x" << std::setw(8) << std::hex << (runtime_address + image_base) << " bytes:";
    for (ZyanU8 i = 0; i < ins.info.length; ++i) {
        if (i > 0) {
            stream << " ";
        }
        stream << std::setw(2) << std::hex << static_cast<uint32_t>(ins_data[i]);
    }
    return stream.str();
}
} // namespace

void Function::disassemble(AsmFormat fmt)
{
    const uint64_t image_base = m_executable.do_add_base() ? m_executable.base_address() : 0;
    const Executable::SectionInfo *section_info = m_executable.find_section(image_base + m_startAddress);

    if (section_info == nullptr) {
        return;
    }

    uint64_t runtime_address = m_startAddress;
    const uint64_t address_offset = section_info->address - image_base;
    ZyanUSize offset = m_startAddress - address_offset;
    const ZyanUSize end_offset = m_endAddress - address_offset;

    if (end_offset - offset > section_info->size) {
        return;
    }

    const uint8_t *section_data = section_info->data;

    ZydisDisassembledInstruction instruction;

    static bool in_jump_table;
    in_jump_table = false;

    // Loop through function once to identify all jumps to local labels and create them.
    while (offset < end_offset) {
        const ZyanStatus status =
            UnasmDisassembleNoFormat(ZYDIS_MACHINE_MODE_LEGACY_32, runtime_address, section_data + offset, 96, &instruction);

        if (!ZYAN_SUCCESS(status)) {
            std::string str = BuildInvalidInstructionString(instruction, section_data + offset, runtime_address, image_base);
            m_dissassembly += str + "\n";
            offset += instruction.info.length;
            runtime_address += instruction.info.length;
            continue;
        }

        uint64_t address;

        if (instruction.info.raw.imm->is_relative) {
            ZydisCalcAbsoluteAddress(&instruction.info, instruction.operands, runtime_address, &address);

            if (address >= m_startAddress && address < m_endAddress) {
                add_symbol(address);
            }
        }

        offset += instruction.info.length;
        runtime_address += instruction.info.length;

        // If instruction is a nop or jmp, could be at an inline jump table.
        if (instruction.info.mnemonic == ZYDIS_MNEMONIC_NOP || instruction.info.mnemonic == ZYDIS_MNEMONIC_JMP) {
            uint64_t next_int = get_le32(section_data + offset);
            bool in_jump_table = false;

            // Naive jump table detection attempt uint32_t representation happens to be in function address space.
            while (next_int >= m_startAddress && next_int < m_endAddress) {
                // If this is first entry of jump table, create label to jump to.
                if (!in_jump_table) {
                    if (runtime_address >= m_startAddress && runtime_address < m_endAddress) {
                        add_symbol(runtime_address);
                    }

                    in_jump_table = true;
                }

                add_symbol(next_int);

                offset += sizeof(uint32_t);
                runtime_address += sizeof(uint32_t);
                next_int = get_le32(section_data + offset);
            }
        }
    }

    offset = m_startAddress - address_offset;
    runtime_address = m_startAddress;
    in_jump_table = false;
    ZydisFormatterStyle style;

    switch (fmt) {
        case FORMAT_MASM:
            style = ZYDIS_FORMATTER_STYLE_INTEL_MASM;
            break;
        case FORMAT_AGAS:
            style = ZYDIS_FORMATTER_STYLE_ATT;
            break;
        case FORMAT_IGAS:
        case FORMAT_DEFAULT:
            style = ZYDIS_FORMATTER_STYLE_INTEL;
            break;
    }

    while (offset < end_offset) {
        const ZyanStatus status = UnasmDisassembleCustom(
            ZYDIS_MACHINE_MODE_LEGACY_32, runtime_address, section_data + offset, 96, &instruction, this, style);

        if (!ZYAN_SUCCESS(status)) {
            std::string str = BuildInvalidInstructionString(instruction, section_data + offset, runtime_address, image_base);
            m_dissassembly += str + "\n";
            offset += instruction.info.length;
            runtime_address += instruction.info.length;
            continue;
        }

        if (m_labels.find(runtime_address) != m_labels.end()) {
            m_dissassembly += m_labels[runtime_address];
            m_dissassembly += ":\n";
        }

        m_dissassembly += "    ";
        m_dissassembly += instruction.text;
        m_dissassembly += '\n';
        offset += instruction.info.length;
        runtime_address += instruction.info.length;

        // If instruction is a nop or jmp, could be at an inline jump table.
        if (instruction.info.mnemonic == ZYDIS_MNEMONIC_NOP || instruction.info.mnemonic == ZYDIS_MNEMONIC_JMP) {
            uint64_t next_int = get_le32(section_data + offset);
            bool in_jump_table = false;

            // Naive jump table detection attempt uint32_t representation happens to be in function address space.
            while (next_int >= m_startAddress && next_int < m_endAddress) {
                // If this is first entry of jump table, create label to jump to.

                if (!in_jump_table) {
                    const Executable::Symbol &symbol = m_executable.get_symbol(runtime_address);

                    if (!symbol.name.empty()) {
                        m_dissassembly += symbol.name;
                        m_dissassembly += ":\n";
                    }

                    in_jump_table = true;
                }

                const Executable::Symbol &symbol = m_executable.get_symbol(next_int);

                if (!symbol.name.empty()) {
                    if (fmt == FORMAT_MASM) {
                        m_dissassembly += "    DWORD ";
                    } else {
                        m_dissassembly += "    .int ";
                    }
                    m_dissassembly += symbol.name;
                    m_dissassembly += "\n";
                }

                offset += sizeof(uint32_t);
                runtime_address += sizeof(uint32_t);
                next_int = get_le32(section_data + offset);
            }
        }
    }
}

void Function::add_symbol(uint64_t address)
{
    if (m_labels.find(address) == m_labels.end()) {
        std::stringstream stream;
        stream << "loc_" << std::hex << address;
        m_labels[address] = stream.str();
        Executable::Symbol symbol;
        symbol.name = m_labels[address];
        symbol.address = address;
        m_executable.add_symbol(symbol);
    }
}

} // namespace unassemblize
