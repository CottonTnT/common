#include <array>
#include <string>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#elif __linux__
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace X::SystemUtil {

inline auto getCurrentHostname()
    -> std::string
{
    auto hostname = std::array<char, 1024> { };
    hostname.fill('\0');

#ifdef _WIN32
    auto size = sizeof(hostname);
    if (GetComputerNameA(hostname.data(), &size)) {
        return std::string { hostname.data() };
    }
#elif __linux__
    if (gethostname(hostname.data(), sizeof(hostname)) == 0) {
        return std::string { hostname.data() };
    }
#endif

    return "UnknownHost";
}

inline auto getThreadId()
    -> std::uint64_t
{
#ifdef _WIN32
    return static_cast<std::uint64_t>(GetCurrentThreadId());
#elif __linux__
    return static_cast<std::uint64_t>(syscall(SYS_gettid));
#endif
    // 其他平台降级方案：使用标准库哈希转换
    return static_cast<std::uint64_t>(std::hash<std::thread::id> { }(std::this_thread::get_id()));
}

inline auto getCachedThreadId()
    -> std::uint64_t
{
    thread_local auto cachedTid = getThreadId();
    return cachedTid;
}
} // namespace X::SystemUtil

// auto main() -> int
// {
//     std::cout << "Current Machine Name: " << X::SystemUtil::getCurrentHostname() << std::endl;

//     std::cout << "Current Thread ID: " << X::SystemUtil::getThreadId() << std::endl;
//     std::cout << "Cached Thread ID: " << X::SystemUtil::getCachedThreadId() << std::endl;
//     return 0;
// }