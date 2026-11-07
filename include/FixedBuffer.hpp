#pragma once
#include <cassert>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <string_view>
#include <system_error>

/* ======================== AppendResult ======================== */

/**
 * @brief 缓冲区写入结果，设计参考 std::to_chars_result。
 *
 * ptr:
 *   成功：指向最后写入字节的下一个位置。
 *   失败：指向操作前的当前写入位置。
 *
 * ec:
 *   成功：std::errc{}。
 *   失败：对应的错误码。
 */
struct AppendResult {
    const char* ptr;
    std::errc ec;

    [[nodiscard]] constexpr explicit operator bool() const noexcept
    {
        return ec == std::errc { };
    }
};

/* ======================== FixedBuffer ======================== */

/**
 * @brief 固定容量字节缓冲区。
 *
 * 特性：
 * - 固定容量，不进行堆内存分配。
 * - 支持顺序写入、回退和重置。
 * - 支持原始字节、string_view 和格式化写入。
 * - 写入失败时不会改变逻辑上的有效数据长度。
 * - 不负责维护字符串结尾的 '\0'。
 * - 非线程安全。
 */
template <std::size_t CAPACITY>
class FixedBuffer {
    static_assert(CAPACITY > 0, "CAPACITY must be greater than zero");

private:
    char data_[CAPACITY] { };
    char* cur_ = data_;

public:
    using size_type = std::size_t;

    FixedBuffer() noexcept = default;

    FixedBuffer(const FixedBuffer&) = delete;
    FixedBuffer& operator=(const FixedBuffer&) = delete;

    FixedBuffer(FixedBuffer&&) = delete;
    FixedBuffer& operator=(FixedBuffer&&) = delete;

    /* ======================== Write ======================== */

    /**
     * @brief 追加原始字节数据。
     *
     * @param buf 数据起始地址。
     * @param len 数据长度。
     *
     * @return AppendResult
     */
    [[nodiscard]] auto append(const char* buf, size_type len) noexcept
        -> AppendResult
    {
        if (len != 0 && buf == nullptr) {
            return { .ptr = buf, .ec = std::errc::invalid_argument };
        }

        const auto available = avail();
        const auto written = std::min(len, available);

        if (written != 0) {
            std::memmove(cur_, buf, written);
            cur_ += written;
        }

        if (written != len) {
            return { .ptr = buf + written, .ec = std::errc::value_too_large };
        }

        return { .ptr = buf + len, .ec = { } };
    }

    /**
     * @brief 追加 string_view。
     */
    [[nodiscard]] auto append(std::string_view sv) noexcept
        -> AppendResult
    {
        return append(sv.data(), sv.size());
    }

    /* ======================== State ======================== */

    /**
     * @brief 重置缓冲区，不清零底层内存。
     */
    void reset() noexcept
    {
        cur_ = data_;
    }

    [[nodiscard]] auto empty() const noexcept -> bool
    {
        return cur_ == data_;
    }

    [[nodiscard]] auto full() const noexcept -> bool
    {
        return cur_ == end();
    }

    /* ======================== Access ======================== */

    /**
     * @brief 获取数据起始地址。
     */
    [[nodiscard]] auto data() const noexcept -> const char*
    {
        return data_;
    }

    /**
     * @brief 获取当前写入位置。
     */
    [[nodiscard]] auto current() noexcept -> char*
    {
        return cur_;
    }

    [[nodiscard]] auto current() const noexcept -> const char*
    {
        return cur_;
    }

    /**
     * @brief 获取缓冲区末尾地址（one-past-the-end）。
     */
    [[nodiscard]] auto end() noexcept -> char*
    {
        return data_ + CAPACITY;
    }

    [[nodiscard]] auto end() const noexcept -> const char*
    {
        return data_ + CAPACITY;
    }

    /**
     * @brief 已写入数据长度。
     */
    [[nodiscard]] auto length() const noexcept -> size_type
    {
        return static_cast<size_type>(cur_ - data_);
    }

    /**
     * @brief 剩余可写入容量。
     */
    [[nodiscard]] auto avail() const noexcept -> size_type
    {
        return static_cast<size_type>(end() - cur_);
    }

    /**
     * @brief 总容量。
     */
    [[nodiscard]] static constexpr auto capacity() noexcept -> size_type
    {
        return CAPACITY;
    }

    /**
     * @brief 获取有效数据视图。
     */
    [[nodiscard]] auto view() const noexcept -> std::string_view
    {
        return { data_, length() };
    }
};
