#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>

namespace realtone {

class Sha256
{
public:
    Sha256();
    void update(const void* data, std::size_t size);
    std::string finishHex();  // lower-case hex digest; the object cannot be reused afterwards

private:
    void block(const std::uint8_t* p);
    std::uint32_t h_[8];
    std::uint8_t buf_[64];
    std::size_t bufLen_ = 0;
    std::uint64_t total_ = 0;
};

std::string sha256Hex(const void* data, std::size_t size);

// Streams the file, so large samples do not need to fit in memory. Returns an
// empty string if the file cannot be read.
std::string sha256File(const std::filesystem::path& path);

}  // namespace realtone
