#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace win2lin::shaders {

enum class DxbcOpcode : uint32_t {
    MOV = 0x01,
    ADD = 0x02,
    MUL = 0x03,
    MAD = 0x04,
    DP3 = 0x0B,
    DP4 = 0x0C,
    SAMPLE = 0xC4,
    LD = 0xC5,
    RET = 0xFF,
    DCL_INPUT = 0xFE,
    DCL_OUTPUT = 0xFD,
    DCL_RESOURCE = 0xFC,
    DCL_CONSTANT_BUFFER = 0xFB,
    IF = 0xE8,
    ELSE = 0xE9,
    ENDIF = 0xEA,
    LOOP = 0xEB,
    ENDLOOP = 0xEC,
    UNKNOWN = 0xFFFFFFFF,
};

struct DxbcOperand {
    uint32_t registerIndex = 0;
    uint32_t componentMask = 0xF; // xyzw
    uint32_t swizzle[4] = {0, 1, 2, 3}; // x, y, z, w
    bool isImmediate = false;
    float immediateValue = 0.0f;
    uint32_t raw[4] = {0, 0, 0, 0};
    uint32_t rawCount = 0;
};

struct DxbcInstruction {
    DxbcOpcode opcode = DxbcOpcode::UNKNOWN;
    uint32_t length = 0;       // number of DWORDs in this instruction
    std::vector<DxbcOperand> operands;
    std::vector<uint32_t> raw; // raw DWORDs for this instruction
};

struct DxbcShader {
    uint32_t version = 0;
    uint32_t shaderType = 0;  // 0xFFFE=VS, 0xFFFD=PS
    uint32_t instructionCount = 0;
    std::vector<DxbcInstruction> instructions;
};

class DxbcParser {
public:
    bool parse(const uint8_t* data, size_t size, DxbcShader& out);
    const std::string& lastError() const { return error_; }

private:
    std::string error_;

    bool parseContainerHeader(const uint8_t* data, size_t size,
                              uint32_t& shaderType, uint32_t& bytecodeOffset);
    bool parseShaderHeader(const uint8_t* data, size_t size, size_t offset,
                           DxbcShader& out, size_t& instrOffset);
    DxbcOpcode decodeOpcode(uint32_t firstDword) const;
    uint32_t decodeInstructionLength(uint32_t firstDword) const;
    bool parseOperands(const uint8_t* data, size_t size, size_t offset,
                       uint32_t remainingDwords, std::vector<DxbcOperand>& operands);
};

} // namespace win2lin::shaders
