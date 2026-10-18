#include "shader_cache.h"
#include "dxbc_to_spirv.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sys/stat.h>

namespace win2lin::shaders {

namespace {

// FNV-1a 64-bit hash.
uint64_t fnv1aHash(const uint8_t* data, size_t size) {
    constexpr uint64_t FNV_OFFSET = 0xcbf29ce484222325ULL;
    constexpr uint64_t FNV_PRIME = 0x100000001b3ULL;
    uint64_t hash = FNV_OFFSET;
    for (size_t i = 0; i < size; ++i) {
        hash ^= data[i];
        hash *= FNV_PRIME;
    }
    return hash;
}

bool dirExists(const std::string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

bool makeDir(const std::string& path) {
    if (dirExists(path)) return true;
    return ::mkdir(path.c_str(), 0755) == 0;
}

} // namespace

ShaderCache::ShaderCache() {
    // Use XDG_CACHE_HOME or ~/.cache
    const char* xdg = getenv("XDG_CACHE_HOME");
    if (xdg && *xdg) {
        cacheDir_ = xdg;
    } else {
        const char* home = getenv("HOME");
        if (home && *home) {
            cacheDir_ = std::string(home) + "/.cache";
        } else {
            cacheDir_ = "/tmp/.cache";
        }
    }
    cacheDir_ += "/nexlink/shaders";
}

ShaderCache::~ShaderCache() {
    clear();
}

uint64_t ShaderCache::hashDxbc(const uint8_t* data, size_t size) const {
    return fnv1aHash(data, size);
}

std::string ShaderCache::cachePath(uint64_t hash) const {
    return cacheDir_ + "/" + std::to_string(hash) + ".spv";
}

bool ShaderCache::ensureCacheDir() const {
    return makeDir(cacheDir_);
}

bool ShaderCache::loadFromDisk(uint64_t hash, std::vector<uint32_t>& spirv) const {
    std::string path = cachePath(hash);
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;

    // Read size first.
    uint32_t size = 0;
    f.read(reinterpret_cast<char*>(&size), 4);
    if (!f || size == 0) return false;

    spirv.resize(size / 4);
    f.read(reinterpret_cast<char*>(spirv.data()), size);
    return f.good();
}

bool ShaderCache::saveToDisk(uint64_t hash,
                             const std::vector<uint32_t>& spirv) const {
    if (!ensureCacheDir()) return false;
    std::string path = cachePath(hash);
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;

    uint32_t size = static_cast<uint32_t>(spirv.size() * 4);
    f.write(reinterpret_cast<const char*>(&size), 4);
    f.write(reinterpret_cast<const char*>(spirv.data()), size);
    return f.good();
}

void ShaderCache::clear() {
    memoryCache_.clear();
}

bool ShaderCache::getOrTranslate(const uint8_t* dxbcData, size_t dxbcSize,
                                 bool isVertexShader,
                                 std::vector<uint32_t>& spirvOut) {
    uint64_t hash = hashDxbc(dxbcData, dxbcSize);

    // Check memory cache.
    auto it = memoryCache_.find(hash);
    if (it != memoryCache_.end()) {
        spirvOut = it->second;
        return true;
    }

    // Check disk cache.
    if (loadFromDisk(hash, spirvOut)) {
        memoryCache_[hash] = spirvOut;
        return true;
    }

    // Not cached - parse and translate.
    DxbcShader dxbc;
    DxbcParser parser;
    if (!parser.parse(dxbcData, dxbcSize, dxbc)) {
        fprintf(stderr,
                "[NEXLINK SHADER] DXBC parse failed for hash=%llu: %s\n",
                static_cast<unsigned long long>(hash),
                parser.lastError().c_str());
        return false;
    }

    DxbcToSpirv translator;
    if (!translator.translate(dxbc, spirvOut)) {
        fprintf(stderr,
                "[NEXLINK SHADER] Translation failed for hash=%llu: %s\n",
                static_cast<unsigned long long>(hash),
                translator.lastError().c_str());
        return false;
    }

    // Cache in memory and on disk.
    memoryCache_[hash] = spirvOut;
    saveToDisk(hash, spirvOut);

    fprintf(stderr,
            "[NEXLINK SHADER] Translated DXBC hash=%llu (%zu bytes -> %zu SPIR-V words)\n",
            static_cast<unsigned long long>(hash), dxbcSize, spirvOut.size());
    return true;
}

} // namespace win2lin::shaders
