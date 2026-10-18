#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "dxbc_parser.h"

namespace win2lin::shaders {

// Shader cache keyed by a hash of the DXBC bytecode.
// Translated SPIR-V is cached in memory and optionally on disk.
class ShaderCache {
public:
    ShaderCache();
    ~ShaderCache();

    // Look up or translate a shader. Returns true if a valid SPIR-V module
    // can be created (spirv is populated with the translated bytes).
    bool getOrTranslate(const uint8_t* dxbcData, size_t dxbcSize,
                        bool isVertexShader,
                        std::vector<uint32_t>& spirvOut);

    // Clear the in-memory cache.
    void clear();

private:
    std::string cacheDir_;
    std::unordered_map<uint64_t, std::vector<uint32_t>> memoryCache_;

    uint64_t hashDxbc(const uint8_t* data, size_t size) const;
    std::string cachePath(uint64_t hash) const;
    bool loadFromDisk(uint64_t hash, std::vector<uint32_t>& spirv) const;
    bool saveToDisk(uint64_t hash, const std::vector<uint32_t>& spirv) const;
    bool ensureCacheDir() const;
};

} // namespace win2lin::shaders
