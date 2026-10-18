#include "dxbc_parser.h"

#include <cstdio>

namespace win2lin::shaders {

bool DxbcParser::parse(const uint8_t* data, size_t size, DxbcShader& out) {
    error_.clear();

    if (size < 28) {
        error_ = "DXBC blob too small for container header";
        return false;
    }

    uint32_t shaderType = 0;
    uint32_t bytecodeOffset = 0;
    if (!parseContainerHeader(data, size, shaderType, bytecodeOffset)) {
        return false;
    }
    out.shaderType = shaderType;

    size_t instrOffset = 0;
    if (!parseShaderHeader(data, size, bytecodeOffset, out, instrOffset)) {
        return false;
    }

    // Parse instructions.
    size_t pos = instrOffset;
    for (uint32_t i = 0; i < out.instructionCount; ++i) {
        if (pos + 4 > size) {
            error_ = "DXBC instruction stream truncated";
            return false;
        }

        uint32_t firstDword = *reinterpret_cast<const uint32_t*>(data + pos);
        DxbcOpcode opcode = decodeOpcode(firstDword);
        uint32_t length = decodeInstructionLength(firstDword);

        if (length == 0 || pos + length * 4 > size) {
            error_ = "DXBC instruction length exceeds remaining data";
            return false;
        }

        DxbcInstruction instr;
        instr.opcode = opcode;
        instr.length = length;
        instr.raw.assign(data + pos, data + pos + length * 4);

        // Parse operands (everything after the first DWORD).
        size_t operandOffset = pos + 4;
        uint32_t operandDwords = length - 1;
        if (!parseOperands(data, size, operandOffset, operandDwords,
                           instr.operands)) {
            error_ = "Failed to parse DXBC operands";
            return false;
        }

        out.instructions.push_back(std::move(instr));
        pos += length * 4;
    }

    return true;
}

bool DxbcParser::parseContainerHeader(const uint8_t* data, size_t size,
                                      uint32_t& shaderType,
                                      uint32_t& bytecodeOffset) {
    // Magic: "DXBC" = 0x43425844 little-endian
    uint32_t magic = *reinterpret_cast<const uint32_t*>(data + 0);
    if (magic != 0x43425844u) {
        error_ = "Invalid DXBC magic number";
        return false;
    }

    uint32_t containerSize = *reinterpret_cast<const uint32_t*>(data + 4);
    if (containerSize > size) {
        error_ = "DXBC container size exceeds blob size";
        return false;
    }

    // uint32_t version = *reinterpret_cast<const uint32_t*>(data + 8);
    shaderType = *reinterpret_cast<const uint32_t*>(data + 12);
    // uint32_t commentCount = *reinterpret_cast<const uint32_t*>(data + 16);
    // uint32_t commentOffset = *reinterpret_cast<const uint32_t*>(data + 20);
    // uint32_t otherCount = *reinterpret_cast<const uint32_t*>(data + 24);
    bytecodeOffset = *reinterpret_cast<const uint32_t*>(data + 28);

    if (bytecodeOffset >= containerSize) {
        error_ = "DXBC bytecode offset out of range";
        return false;
    }

    return true;
}

bool DxbcParser::parseShaderHeader(const uint8_t* data, size_t size, size_t offset,
                                   DxbcShader& out, size_t& instrOffset) {
    if (offset + 8 > size) {
        error_ = "DXBC shader header truncated";
        return false;
    }

    out.version = *reinterpret_cast<const uint32_t*>(data + offset);
    out.instructionCount = *reinterpret_cast<const uint32_t*>(data + offset + 4);

    // The instruction stream starts after the shader header (8 bytes for
    // version + instruction count). Some DXBC versions have more header
    // fields; for our minimal parser we assume 8-byte header.
    instrOffset = offset + 8;
    return true;
}

DxbcOpcode DxbcParser::decodeOpcode(uint32_t firstDword) const {
    uint32_t op = firstDword & 0x1FFu; // bits 0-8
    switch (op) {
        case 0x01: return DxbcOpcode::MOV;
        case 0x02: return DxbcOpcode::ADD;
        case 0x03: return DxbcOpcode::MUL;
        case 0x04: return DxbcOpcode::MAD;
        case 0x0B: return DxbcOpcode::DP3;
        case 0x0C: return DxbcOpcode::DP4;
        case 0xC4: return DxbcOpcode::SAMPLE;
        case 0xC5: return DxbcOpcode::LD;
        case 0xFF: return DxbcOpcode::RET;
        case 0xFE: return DxbcOpcode::DCL_INPUT;
        case 0xFD: return DxbcOpcode::DCL_OUTPUT;
        case 0xFC: return DxbcOpcode::DCL_RESOURCE;
        case 0xFB: return DxbcOpcode::DCL_CONSTANT_BUFFER;
        case 0xE8: return DxbcOpcode::IF;
        case 0xE9: return DxbcOpcode::ELSE;
        case 0xEA: return DxbcOpcode::ENDIF;
        case 0xEB: return DxbcOpcode::LOOP;
        case 0xEC: return DxbcOpcode::ENDLOOP;
        default: return DxbcOpcode::UNKNOWN;
    }
}

uint32_t DxbcParser::decodeInstructionLength(uint32_t firstDword) const {
    // Bits 9-23: instruction length in DWORDs (including this one)
    return (firstDword >> 9) & 0x7FFFu;
}

bool DxbcParser::parseOperands(const uint8_t* data, size_t size, size_t offset,
                               uint32_t remainingDwords,
                               std::vector<DxbcOperand>& operands) {
    size_t pos = offset;
    uint32_t dwordsLeft = remainingDwords;

    while (dwordsLeft > 0) {
        if (pos + 4 > size) {
            error_ = "DXBC operand stream truncated";
            return false;
        }

        uint32_t operandHeader = *reinterpret_cast<const uint32_t*>(data + pos);
        uint32_t operandType = operandHeader & 0xFu; // bits 0-3

        DxbcOperand op;
        op.raw[0] = operandHeader;
        op.rawCount = 1;

        if (operandType == 0) { // Register operand
            // Bits 4-7: component mask
            op.componentMask = (operandHeader >> 4) & 0xFu;
            // Bits 8-15: register index (for simple cases)
            op.registerIndex = (operandHeader >> 8) & 0xFFu;
            // Swizzle: bits 16-19 (x), 20-23 (y), 24-27 (z), 28-31 (w)
            op.swizzle[0] = (operandHeader >> 16) & 0x3u;
            op.swizzle[1] = (operandHeader >> 20) & 0x3u;
            op.swizzle[2] = (operandHeader >> 24) & 0x3u;
            op.swizzle[3] = (operandHeader >> 28) & 0x3u;

            // Check for extended operand (second DWORD) - indicated by
            // bit 10 (index dimension flag).
            bool extended = (operandHeader >> 10) & 0x1u;
            if (extended && dwordsLeft >= 2 && pos + 8 <= size) {
                uint32_t operandDword2 = *reinterpret_cast<const uint32_t*>(data + pos + 4);
                op.raw[1] = operandDword2;
                op.rawCount = 2;
                pos += 8;
                dwordsLeft -= 2;
            } else {
                pos += 4;
                dwordsLeft -= 1;
            }
        } else if (operandType == 1) { // Immediate operand
            op.isImmediate = true;
            // Bits 8-31: immediate value (as float bits)
            op.immediateValue = *reinterpret_cast<const float*>(&operandHeader);
            pos += 4;
            dwordsLeft -= 1;
        } else {
            // Unknown operand type, skip one DWORD
            pos += 4;
            dwordsLeft -= 1;
        }

        operands.push_back(std::move(op));
    }

    return true;
}

} // namespace win2lin::shaders
