#include "dxbc_to_spirv.h"

#include <cstring>

namespace win2lin::shaders {

namespace {

// SPIR-V opcodes (verified against spirv-as output).
constexpr uint32_t SpvOpCapability = 17;
constexpr uint32_t SpvOpMemoryModel = 14;
constexpr uint32_t SpvOpEntryPoint = 15;
constexpr uint32_t SpvOpSource = 3;
constexpr uint32_t SpvOpName = 5;
constexpr uint32_t SpvOpDecorate = 71;
constexpr uint32_t SpvOpTypeVoid = 19;
constexpr uint32_t SpvOpTypeFunction = 33;
constexpr uint32_t SpvOpTypeFloat = 22;
constexpr uint32_t SpvOpTypeVector = 23;
constexpr uint32_t SpvOpTypePointer = 32;
constexpr uint32_t SpvOpConstant = 43;
constexpr uint32_t SpvOpConstantComposite = 44;
constexpr uint32_t SpvOpVariable = 59;
constexpr uint32_t SpvOpFunction = 54;
constexpr uint32_t SpvOpLabel = 248;
constexpr uint32_t SpvOpLoad = 61;
constexpr uint32_t SpvOpStore = 62;
constexpr uint32_t SpvOpReturn = 253;
constexpr uint32_t SpvOpFunctionEnd = 56;
constexpr uint32_t SpvOpFAdd = 129;
constexpr uint32_t SpvOpFMul = 133;
constexpr uint32_t SpvOpFma = 145;
constexpr uint32_t SpvOpDot = 148;
constexpr uint32_t SpvOpCopyObject = 83;
constexpr uint32_t SpvOpCompositeConstruct = 80;
constexpr uint32_t SpvOpCompositeExtract = 81;
constexpr uint32_t SpvOpVectorShuffle = 79;

constexpr uint32_t SpvCapShader = 1;
constexpr uint32_t SpvAddrLogical = 0;
constexpr uint32_t SpvMemModelGLSL450 = 1;
constexpr uint32_t SpvStorageInput = 1;
constexpr uint32_t SpvStorageOutput = 2;
constexpr uint32_t SpvStorageUniform = 2;
constexpr uint32_t SpvStorageUniformConstant = 4;
constexpr uint32_t SpvDecorationLocation = 30;
constexpr uint32_t SpvDecorationBuiltIn = 35;
constexpr uint32_t SpvBuiltInPosition = 4;
constexpr uint32_t SpvExecModelVertex = 1;
constexpr uint32_t SpvExecModelFragment = 4;

template <typename T>
uint32_t packWord(T value) {
    uint32_t w = 0;
    std::memcpy(&w, &value, std::min(sizeof(T), sizeof(uint32_t)));
    return w;
}

} // namespace

void DxbcToSpirv::emit(std::vector<uint32_t>& spirv, uint32_t word) {
    spirv.push_back(word);
}

void DxbcToSpirv::emitOp(std::vector<uint32_t>& spirv, uint32_t opcode,
                         uint32_t resultType, uint32_t resultId,
                         std::initializer_list<uint32_t> operands) {
    uint32_t wordCount = 2 + static_cast<uint32_t>(operands.size());
    uint32_t word0 = (wordCount << 16) | opcode;
    spirv.push_back(word0);
    spirv.push_back(resultType);
    spirv.push_back(resultId);
    for (uint32_t o : operands) spirv.push_back(o);
}

uint32_t DxbcToSpirv::getOrCreateRegisterId(uint32_t regIndex,
                                            uint32_t component) {
    return 100u + regIndex * 4u + component;
}

bool DxbcToSpirv::translate(const DxbcShader& dxbc,
                            std::vector<uint32_t>& spirv) {
    spirv.clear();
    nextId_ = 1;
    error_.clear();

    bool isVertex = (dxbc.shaderType == 0xFFFEu);

    // ------------------------------------------------------------------
    // Pass 1: Scan DXBC instructions to collect interface variable IDs
    // (needed by OpEntryPoint before types are emitted).
    // ------------------------------------------------------------------
    std::vector<uint32_t> inputVarIds;
    std::vector<uint32_t> outputVarIds;
    std::vector<std::pair<uint32_t, std::string>> allNames;

    for (const auto& instr : dxbc.instructions) {
        if (instr.opcode == DxbcOpcode::DCL_INPUT) {
            uint32_t varId = freshId();
            inputVarIds.push_back(varId);
            uint32_t loc = instr.operands.empty() ? 0u : instr.operands[0].registerIndex;
            allNames.emplace_back(varId, "v" + std::to_string(loc));
        } else if (instr.opcode == DxbcOpcode::DCL_OUTPUT) {
            uint32_t varId = freshId();
            outputVarIds.push_back(varId);
            uint32_t loc = instr.operands.empty() ? 0u : instr.operands[0].registerIndex;
            allNames.emplace_back(varId, "o" + std::to_string(loc));
        }
    }

    // ------------------------------------------------------------------
    // SPIR-V Header
    // ------------------------------------------------------------------
    emit(spirv, 0x07230203u); // Magic
    emit(spirv, 0x00010300u); // Version 1.3
    emit(spirv, 0x00000000u); // Generator
    emit(spirv, 0u);          // Bound (patched later)
    emit(spirv, 0x00000000u); // Schema

    // OpCapability Shader
    {
        uint32_t wc = 2;
        spirv.push_back((wc << 16) | SpvOpCapability);
        spirv.push_back(SpvCapShader);
    }

    // OpMemoryModel Logical GLSL450
    {
        uint32_t wc = 3;
        spirv.push_back((wc << 16) | SpvOpMemoryModel);
        spirv.push_back(SpvAddrLogical);
        spirv.push_back(SpvMemModelGLSL450);
    }

    // ------------------------------------------------------------------
    // OpEntryPoint (must come before types in SPIR-V)
    // ------------------------------------------------------------------
    uint32_t mainFnId = freshId();
    uint32_t execModel = isVertex ? SpvExecModelVertex : SpvExecModelFragment;
    const char* entryName = "main";
    uint32_t nameLen = 4u;
    uint32_t strWords = (nameLen + 3u) / 4u;
    uint32_t wc = 3u + strWords +
                  static_cast<uint32_t>(inputVarIds.size()) +
                  static_cast<uint32_t>(outputVarIds.size());
    spirv.push_back((wc << 16) | SpvOpEntryPoint);
    spirv.push_back(execModel);
    spirv.push_back(mainFnId);
    // Name string (null-terminated, padded).
    uint8_t nameBuf[8] = {0};
    std::memcpy(nameBuf, entryName, nameLen);
    for (uint32_t w = 0; w < strWords; ++w) {
        uint32_t packed = 0;
        std::memcpy(&packed, nameBuf + w * 4,
                    std::min(nameLen - w * 4, uint32_t(4)));
        spirv.push_back(packed);
    }
    for (uint32_t v : inputVarIds) spirv.push_back(v);
    for (uint32_t v : outputVarIds) spirv.push_back(v);

    // ------------------------------------------------------------------
    // OpSource
    // ------------------------------------------------------------------
    {
        uint32_t wc = 3;
        spirv.push_back((wc << 16) | SpvOpSource);
        spirv.push_back(0); // HLSL
        spirv.push_back(1); // Version
    }

    // ------------------------------------------------------------------
    // OpName (entry point + interface variables)
    // ------------------------------------------------------------------
    {
        uint32_t strBytes = nameLen + 1; // null-terminated
        uint32_t strWords = (strBytes + 3u) / 4u;
        uint32_t wc = 2u + strWords;
        spirv.push_back((wc << 16) | SpvOpName);
        spirv.push_back(mainFnId);
        uint8_t strBuf[8] = {0};
        std::memcpy(strBuf, entryName, nameLen);
        for (uint32_t w = 0; w < strWords; ++w) {
            uint32_t packed = 0;
            std::memcpy(&packed, strBuf + w * 4,
                        std::min(strBytes - w * 4, uint32_t(4)));
            spirv.push_back(packed);
        }
    }
    for (const auto& [id, n] : allNames) {
        uint32_t nLen = static_cast<uint32_t>(n.size());
        uint32_t strBytes = nLen + 1;
        uint32_t strWords = (strBytes + 3u) / 4u;
        uint32_t wc = 2u + strWords;
        spirv.push_back((wc << 16) | SpvOpName);
        spirv.push_back(id);
        uint8_t strBuf[8] = {0};
        std::memcpy(strBuf, n.data(), nLen);
        for (uint32_t w = 0; w < strWords; ++w) {
            uint32_t packed = 0;
            std::memcpy(&packed, strBuf + w * 4,
                        std::min(strBytes - w * 4, uint32_t(4)));
            spirv.push_back(packed);
        }
    }

    // ------------------------------------------------------------------
    // Types
    // ------------------------------------------------------------------
    uint32_t voidTypeId = freshId();
    {
        uint32_t wc = 2;
        spirv.push_back((wc << 16) | SpvOpTypeVoid);
        spirv.push_back(voidTypeId);
    }
    uint32_t voidFnTypeId = freshId();
    {
        uint32_t wc = 3;
        spirv.push_back((wc << 16) | SpvOpTypeFunction);
        spirv.push_back(voidFnTypeId);
        spirv.push_back(voidTypeId);
    }
    uint32_t floatTypeId = freshId();
    {
        uint32_t wc = 3;
        spirv.push_back((wc << 16) | SpvOpTypeFloat);
        spirv.push_back(floatTypeId);
        spirv.push_back(32);
    }
    uint32_t vec3TypeId = freshId();
    {
        uint32_t wc = 4;
        spirv.push_back((wc << 16) | SpvOpTypeVector);
        spirv.push_back(vec3TypeId);
        spirv.push_back(floatTypeId);
        spirv.push_back(3);
    }
    uint32_t vec4TypeId = freshId();
    {
        uint32_t wc = 4;
        spirv.push_back((wc << 16) | SpvOpTypeVector);
        spirv.push_back(vec4TypeId);
        spirv.push_back(floatTypeId);
        spirv.push_back(4);
    }
    uint32_t ptrInputVec3Id = freshId();
    {
        uint32_t wc = 4;
        spirv.push_back((wc << 16) | SpvOpTypePointer);
        spirv.push_back(ptrInputVec3Id);
        spirv.push_back(SpvStorageInput);
        spirv.push_back(vec3TypeId);
    }
    uint32_t ptrOutputVec4Id = freshId();
    {
        uint32_t wc = 4;
        spirv.push_back((wc << 16) | SpvOpTypePointer);
        spirv.push_back(ptrOutputVec4Id);
        spirv.push_back(SpvStorageOutput);
        spirv.push_back(vec4TypeId);
    }

    // Constant: 1.0
    uint32_t oneConstId = freshId();
    {
        uint32_t wc = 4;
        spirv.push_back((wc << 16) | SpvOpConstant);
        spirv.push_back(floatTypeId);
        spirv.push_back(oneConstId);
        float one = 1.0f;
        spirv.push_back(packWord(one));
    }
    // Constant: 0.0
    uint32_t zeroConstId = freshId();
    {
        uint32_t wc = 4;
        spirv.push_back((wc << 16) | SpvOpConstant);
        spirv.push_back(floatTypeId);
        spirv.push_back(zeroConstId);
        float zero = 0.0f;
        spirv.push_back(packWord(zero));
    }

    // ------------------------------------------------------------------
    // Variables + OpDecorate
    // ------------------------------------------------------------------
    size_t iv = 0, ov = 0;
    for (const auto& instr : dxbc.instructions) {
        if (instr.opcode == DxbcOpcode::DCL_INPUT) {
            uint32_t varId = inputVarIds[iv++];
            {
                uint32_t wc = 4;
                spirv.push_back((wc << 16) | SpvOpVariable);
                spirv.push_back(ptrInputVec3Id);
                spirv.push_back(varId);
                spirv.push_back(SpvStorageInput);
            }
            uint32_t loc = instr.operands.empty() ? 0u : instr.operands[0].registerIndex;
            {
                uint32_t wc = 4;
                spirv.push_back((wc << 16) | SpvOpDecorate);
                spirv.push_back(varId);
                spirv.push_back(SpvDecorationLocation);
                spirv.push_back(loc);
            }
        } else if (instr.opcode == DxbcOpcode::DCL_OUTPUT) {
            uint32_t varId = outputVarIds[ov++];
            {
                uint32_t wc = 4;
                spirv.push_back((wc << 16) | SpvOpVariable);
                spirv.push_back(ptrOutputVec4Id);
                spirv.push_back(varId);
                spirv.push_back(SpvStorageOutput);
            }
            uint32_t loc = instr.operands.empty() ? 0u : instr.operands[0].registerIndex;
            {
                uint32_t wc = 4;
                spirv.push_back((wc << 16) | SpvOpDecorate);
                spirv.push_back(varId);
                spirv.push_back(SpvDecorationLocation);
                spirv.push_back(loc);
            }
            if (isVertex && loc == 0) {
                uint32_t wc = 4;
                spirv.push_back((wc << 16) | SpvOpDecorate);
                spirv.push_back(varId);
                spirv.push_back(SpvDecorationBuiltIn);
                spirv.push_back(SpvBuiltInPosition);
            }
        }
    }

    // ------------------------------------------------------------------
    // Functions
    // ------------------------------------------------------------------
    {
        uint32_t wc = 5;
        spirv.push_back((wc << 16) | SpvOpFunction);
        spirv.push_back(voidTypeId);
        spirv.push_back(mainFnId);
        spirv.push_back(0); // Function control: None
        spirv.push_back(voidFnTypeId);
    }

    // Entry block label.
    uint32_t labelId = freshId();
    {
        uint32_t wc = 2;
        spirv.push_back((wc << 16) | SpvOpLabel);
        spirv.push_back(labelId);
    }

    // ------------------------------------------------------------------
    // Translate DXBC instructions
    // ------------------------------------------------------------------
    uint32_t currentInputVar = inputVarIds.empty() ? 0 : inputVarIds[0];
    uint32_t currentOutputVar = outputVarIds.empty() ? 0 : outputVarIds[0];

    for (const auto& instr : dxbc.instructions) {
        switch (instr.opcode) {
            case DxbcOpcode::DCL_INPUT:
            case DxbcOpcode::DCL_OUTPUT:
                // Already declared above.
                break;

            case DxbcOpcode::MOV: {
                if (instr.operands.size() < 2) {
                    error_ = "MOV requires at least 2 operands";
                    return false;
                }
                uint32_t dstVar = currentOutputVar;
                uint32_t srcVar = currentInputVar;

                // Load source vec3.
                uint32_t srcLoaded = freshId();
                {
                    uint32_t wc = 4;
                    spirv.push_back((wc << 16) | SpvOpLoad);
                    spirv.push_back(vec3TypeId);
                    spirv.push_back(srcLoaded);
                    spirv.push_back(srcVar);
                }

                // Extract components.
                uint32_t compX = freshId();
                {
                    uint32_t wc = 5;
                    spirv.push_back((wc << 16) | SpvOpCompositeExtract);
                    spirv.push_back(floatTypeId);
                    spirv.push_back(compX);
                    spirv.push_back(srcLoaded);
                    spirv.push_back(0u);
                }
                uint32_t compY = freshId();
                {
                    uint32_t wc = 5;
                    spirv.push_back((wc << 16) | SpvOpCompositeExtract);
                    spirv.push_back(floatTypeId);
                    spirv.push_back(compY);
                    spirv.push_back(srcLoaded);
                    spirv.push_back(1u);
                }
                uint32_t compZ = freshId();
                {
                    uint32_t wc = 5;
                    spirv.push_back((wc << 16) | SpvOpCompositeExtract);
                    spirv.push_back(floatTypeId);
                    spirv.push_back(compZ);
                    spirv.push_back(srcLoaded);
                    spirv.push_back(2u);
                }

                // Construct vec4(x, y, z, 1.0).
                uint32_t vec4Val = freshId();
                {
                    uint32_t wc = 7;
                    spirv.push_back((wc << 16) | SpvOpCompositeConstruct);
                    spirv.push_back(vec4TypeId);
                    spirv.push_back(vec4Val);
                    spirv.push_back(compX);
                    spirv.push_back(compY);
                    spirv.push_back(compZ);
                    spirv.push_back(oneConstId);
                }

                // Store to output.
                {
                    uint32_t wc = 3;
                    spirv.push_back((wc << 16) | SpvOpStore);
                    spirv.push_back(dstVar);
                    spirv.push_back(vec4Val);
                }
                break;
            }

            case DxbcOpcode::RET: {
                uint32_t wc = 1;
                spirv.push_back((wc << 16) | SpvOpReturn);
                break;
            }

            default:
                error_ = "Unsupported DXBC opcode: " +
                         std::to_string(static_cast<uint32_t>(instr.opcode));
                return false;
        }
    }

    // OpFunctionEnd.
    {
        uint32_t wc = 1;
        spirv.push_back((wc << 16) | SpvOpFunctionEnd);
    }

    // Patch bound.
    spirv[3] = nextId_;

    return true;
}

} // namespace win2lin::shaders
