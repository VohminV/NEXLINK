#pragma once

#include <cstdint>
#include <vector>

#include "dxbc_parser.h"

namespace win2lin::shaders {

class DxbcToSpirv {
public:
    // Translate a parsed DXBC shader into SPIR-V binary.
    // Returns true on success, with spirv populated.
    bool translate(const DxbcShader& dxbc, std::vector<uint32_t>& spirv);

    const std::string& lastError() const { return error_; }

private:
    std::string error_;
    uint32_t nextId_ = 1;

    uint32_t freshId() { return nextId_++; }

    // SPIR-V emission helpers
    void emit(std::vector<uint32_t>& spirv, uint32_t word);
    void emitOp(std::vector<uint32_t>& spirv, uint32_t opcode, uint32_t resultType,
                uint32_t resultId, std::initializer_list<uint32_t> operands);

    // Translation phases
    bool emitHeader(std::vector<uint32_t>& spirv, bool isVertexShader);
    bool emitDeclarations(std::vector<uint32_t>& spirv, const DxbcShader& dxbc);
    bool emitInstructions(std::vector<uint32_t>& spirv, const DxbcShader& dxbc);
    bool translateInstruction(std::vector<uint32_t>& spirv,
                              const DxbcInstruction& instr,
                              bool isVertexShader);

    // Register -> SPIR-V ID mapping
    uint32_t getOrCreateRegisterId(uint32_t regIndex, uint32_t component);
};

} // namespace win2lin::shaders
