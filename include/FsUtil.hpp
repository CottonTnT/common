
#pragma once

#include <FixedBuffer.hpp>
#include <XType.hpp>
#include <filesystem>
#include <fstream>

namespace X::FsUtil {

template <std::size_t BufferSize = 64_kb>
class AppendFileStream {

private:
    static constexpr std::size_t BUFFER_SIZE = BufferSize > 64_kb ? BufferSize : 64_kb;

    std::fstream fstream_;
    std::array<char, BUFFER_SIZE> buf_;
    std::size_t writtenBytes_;

public:
    explicit AppendFileStream(const std::filesystem::path& filePath)
        : fstream_ { filePath, std::ios::app | std::ios::binary }
        , writtenBytes_ { 0 }
    {
        assert(fstream_.is_open() && std::filesystem::is_regular_file(filePath));
        fstream_.rdbuf()->pubsetbuf(buf_.data(), buf_.size());
    }

    void append(const char* msg, std::size_t len)
    {
        fstream_.write(msg, len);
        writtenBytes_ += len;
    }

    void flush()
    {
        fstream_.flush();
    }

    [[nodiscard]] auto writtenBytes() const noexcept
        -> std::size_t
    {
        return writtenBytes_;
    }
};
} // namespace X::FsUtil