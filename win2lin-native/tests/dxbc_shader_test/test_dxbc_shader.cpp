// test_dxbc_shader.cpp - Unit test for DXBC parser + SPIR-V translator + shader cache.
//
// Builds a minimal DXBC vertex shader blob by hand (no fxc available on Linux),
// then verifies the full translation pipeline: parse -> translate -> cache.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include "shaders/dxbc_parser.h"
#include "shaders/dxbc_to_spirv.h"
#include "shaders/shader_cache.h"

using namespace win2lin::shaders;

static int g_checks = 0;
static int g_failures = 0;

static void check(bool cond, const std::string& what) {
    ++g_checks;
    if (cond) {
        std::printf("  [ OK ] %s\n", what.c_str());
    } else {
        ++g_failures;
        std::printf("  [FAIL] %s\n", what.c_str());
    }
}

// Build a minimal DXBC vertex shader:
//   DCL_INPUT v0.xyz
//   DCL_OUTPUT o0.xyzw
//   MOV o0, v0        (pass-through, w=1.0 handled by translator)
//   RET
static std::vector<uint8_t> buildTestDxbcVs() {
    std::vector<uint8_t> blob;

    auto pushU32 = [&blob](uint32_t v) {
        const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&v);
        blob.insert(blob.end(), bytes, bytes + 4);
    };

    // ---- DXBC container header (32 bytes) ----
    pushU32(0x43425844u);           // magic "DXBC"
    // containerSize patched later
    size_t sizePos = blob.size();
    pushU32(0);                     // placeholder
    pushU32(0x00010000u);           // version
    pushU32(0x0000FFFEu);           // shaderType = vertex shader
    pushU32(0u);                    // commentCount
    pushU32(32u);                   // commentOffset
    pushU32(1u);                    // otherCount
    pushU32(32u);                   // otherOffset (shader bytecode starts at 32)

    // ---- Shader bytecode header (8 bytes at offset 32) ----
    pushU32(0xFFFF0500u);           // shader version (SM 5.0)
    pushU32(4u);                    // instructionCount

    // ---- Instruction 1: DCL_INPUT v0.xyz ----
    // DWORD 0: opcode=0xFE (DCL_INPUT), length=2 DWORDs
    pushU32(0xFEu | (2u << 9));
    // DWORD 1: operand - register v0, mask xyz (0x7), swizzle x=0,y=1,z=2,w=3
    pushU32(0u | (0x7u << 4) | (0u << 8) | (0u << 16) | (1u << 20) | (2u << 24) | (3u << 28));

    // ---- Instruction 2: DCL_OUTPUT o0.xyzw ----
    // DWORD 0: opcode=0xFD (DCL_OUTPUT), length=2 DWORDs
    pushU32(0xFDu | (2u << 9));
    // DWORD 1: operand - register o0, mask xyzw (0xF), swizzle 0,1,2,3
    pushU32(0u | (0xFu << 4) | (0u << 8) | (0u << 16) | (1u << 20) | (2u << 24) | (3u << 28));

    // ---- Instruction 3: MOV o0, v0 ----
    // DWORD 0: opcode=0x01 (MOV), length=3 DWORDs (opcode + 2 operands)
    pushU32(0x01u | (3u << 9));
    // DWORD 1: dst operand (o0)
    pushU32(0u | (0xFu << 4) | (0u << 8) | (0u << 16) | (1u << 20) | (2u << 24) | (3u << 28));
    // DWORD 2: src operand (v0)
    pushU32(0u | (0xFu << 4) | (0u << 8) | (0u << 16) | (1u << 20) | (2u << 24) | (3u << 28));

    // ---- Instruction 4: RET ----
    // DWORD 0: opcode=0xFF (RET), length=1 DWORD
    pushU32(0xFFu | (1u << 9));

    // Patch container size.
    uint32_t containerSize = static_cast<uint32_t>(blob.size());
    std::memcpy(blob.data() + sizePos, &containerSize, 4);

    return blob;
}

int main() {
    std::printf("=== DXBC Shader Translation Unit Test ===\n\n");

    // ---- Build test DXBC blob ----
    auto dxbc = buildTestDxbcVs();
    std::printf("DXBC blob size: %zu bytes\n", dxbc.size());

    // ---- Test 1: DXBC Parser ----
    std::printf("\n--- DXBC Parser ---\n");
    DxbcShader dxbcShader;
    DxbcParser parser;
    bool parsed = parser.parse(dxbc.data(), dxbc.size(), dxbcShader);
    check(parsed, "DXBC parse succeeds");
    if (parsed) {
        check(dxbcShader.shaderType == 0x0000FFFEu, "Shader type is vertex shader");
        check(dxbcShader.instructionCount == 4, "Instruction count is 4");
        check(dxbcShader.instructions.size() == 4, "4 instructions parsed");

        // Check opcodes.
        check(dxbcShader.instructions[0].opcode == DxbcOpcode::DCL_INPUT,
              "Instr 0 is DCL_INPUT");
        check(dxbcShader.instructions[1].opcode == DxbcOpcode::DCL_OUTPUT,
              "Instr 1 is DCL_OUTPUT");
        check(dxbcShader.instructions[2].opcode == DxbcOpcode::MOV,
              "Instr 2 is MOV");
        check(dxbcShader.instructions[3].opcode == DxbcOpcode::RET,
              "Instr 3 is RET");
    }

    // ---- Test 2: DXBC -> SPIR-V Translator ----
    std::printf("\n--- DXBC to SPIR-V Translator ---\n");
    std::vector<uint32_t> spirv;
    DxbcToSpirv translator;
    bool translated = translator.translate(dxbcShader, spirv);
    check(translated, "DXBC to SPIR-V translation succeeds");
    if (translated) {
        check(!spirv.empty(), "SPIR-V output is non-empty");
        check(spirv[0] == 0x07230203u, "SPIR-V magic number correct");
        
        // Dump SPIR-V words for debugging (skip 5-word header)
        std::printf("[DXBC DEBUG] SPIR-V words: %zu\n", spirv.size());
        for (size_t i = 5; i < spirv.size(); ++i) {
            uint32_t w = spirv[i];
            uint32_t count = (w >> 16) & 0xFFFF;
            uint32_t opcode = w & 0xFFFF;
            std::printf("[DXBC DEBUG] word[%zu]: count=%u opcode=%u ops=", i, count, opcode);
            for (uint32_t j = 1; j < count && i + j < spirv.size(); ++j) {
                std::printf("0x%08x ", spirv[i + j]);
            }
            std::printf("\n");
            i += count - 1;
        }
        
        // Verify key structural elements
        // Word 5: OpCapability Shader
        check(spirv[5] == ((2u << 16) | 17u), "OpCapability Shader at word 5");
        // Word 7: OpMemoryModel Logical GLSL450
        check(spirv[7] == ((3u << 16) | 14u), "OpMemoryModel at word 7");
        // Word 10: OpEntryPoint with Vertex execution model
        check(spirv[10] == ((6u << 16) | 15u), "OpEntryPoint at word 10");
        check(spirv[11] == 1u, "OpEntryPoint execution model = Vertex (1)");
        // Word 29: OpTypeVoid
        check(spirv[29] == ((2u << 16) | 19u), "OpTypeVoid at word 29");
        // Find OpFunction and OpLabel somewhere after types
        bool foundFunc = false, foundLabel = false, foundReturn = false, foundFuncEnd = false;
        for (size_t i = 5; i < spirv.size(); ++i) {
            uint32_t w = spirv[i];
            uint32_t opcode = w & 0xFFFF;
            if (opcode == 54) foundFunc = true;  // OpFunction
            if (opcode == 248) foundLabel = true; // OpLabel
            if (opcode == 253) foundReturn = true; // OpReturn
            if (opcode == 56) foundFuncEnd = true; // OpFunctionEnd
            uint32_t count = (w >> 16) & 0xFFFF;
            i += count - 1;
        }
        check(foundFunc, "OpFunction present");
        check(foundLabel, "OpLabel present");
        check(foundReturn, "OpReturn present");
        check(foundFuncEnd, "OpFunctionEnd present");
    }

    // ---- Test 3: Shader Cache (memory + disk) ----
    std::printf("\n--- Shader Cache ---\n");
    ShaderCache cache;
    std::vector<uint32_t> cachedSpirv;
    bool cached = cache.getOrTranslate(dxbc.data(), dxbc.size(), true, cachedSpirv);
    check(cached, "Shader cache getOrTranslate succeeds");
    if (cached) {
        check(cachedSpirv == spirv, "Cached SPIR-V matches translated SPIR-V");

        // Second call should hit memory cache.
        std::vector<uint32_t> cachedSpirv2;
        bool cached2 = cache.getOrTranslate(dxbc.data(), dxbc.size(), true, cachedSpirv2);
        check(cached2, "Shader cache second call succeeds (memory cache)");
        check(cachedSpirv2 == spirv, "Memory cache returns same SPIR-V");

        // Disk cache: new cache instance should load from disk.
        ShaderCache cache2;
        std::vector<uint32_t> cachedSpirv3;
        bool cached3 = cache2.getOrTranslate(dxbc.data(), dxbc.size(), true, cachedSpirv3);
        check(cached3, "Shader cache loads from disk (second instance)");
        check(cachedSpirv3 == spirv, "Disk cache returns same SPIR-V");
    }

    // ---- Test 4: Unsupported opcode falls back gracefully ----
    std::printf("\n--- Error Handling ---\n");
    // Build a DXBC with an unsupported opcode (e.g., DP3).
    std::vector<uint8_t> badDxbc;
    auto pushU32b = [&badDxbc](uint32_t v) {
        const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&v);
        badDxbc.insert(badDxbc.end(), bytes, bytes + 4);
    };
    pushU32b(0x43425844u);       // magic
    pushU32b(32u);               // containerSize
    pushU32b(0x00010000u);       // version
    pushU32b(0x0000FFFEu);       // shaderType = VS
    pushU32b(0u);                // commentCount
    pushU32b(32u);               // commentOffset
    pushU32b(1u);                // otherCount
    pushU32b(32u);               // otherOffset
    pushU32b(0xFFFF0500u);       // shader version
    pushU32b(1u);                // instructionCount
    // DP3 opcode (0x0B), length=1
    pushU32b(0x0Bu | (1u << 9));

    std::vector<uint32_t> badSpirv;
    bool badResult = cache.getOrTranslate(badDxbc.data(), badDxbc.size(), true, badSpirv);
    check(!badResult, "Translation of unsupported opcode fails gracefully");
    check(badSpirv.empty(), "No SPIR-V produced for unsupported opcode");

    // ---- Summary ----
    std::printf("\n=== %d/%d checks passed ===\n",
                g_checks - g_failures, g_checks);
    return g_failures == 0 ? 0 : 1;
}
