#pragma once

#include <concepts>
#include <memory>

namespace X {

template <typename T>
class Singleton {

private:
    Singleton(const Singleton&) = delete;
    Singleton& operator=(const Singleton&) = delete;
    Singleton(Singleton&&) = delete;
    Singleton& operator=(Singleton&&) = delete;

protected:
    Singleton() = default;
    ~Singleton() = default;

public:
    template <typename U = void>
        requires std::default_initializable<T>
    static auto getInstance()
        -> std::shared_ptr<T>
    {
        static auto instance = std::make_shared<T>();
        return instance;
    }

    static auto getInstanceRef()
        -> T&
    {
        return *(getInstance().get());
    }
};

template <typename T>
class SingletonWithConstructToken {
protected:
    // 只有 BaseFactory 和 Derived 可以访问 Key 的构造函数
    class ConstructToken {
        friend class SingletonWithConstructToken<T>;

    private:
        ConstructToken() = default;
    };
    SingletonWithConstructToken() = default;
    ~SingletonWithConstructToken() = default;

public:
    // 禁用拷贝和移动，确保单例语义
    SingletonWithConstructToken(const SingletonWithConstructToken&) = delete;
    SingletonWithConstructToken& operator=(const SingletonWithConstructToken&) = delete;
    SingletonWithConstructToken(SingletonWithConstructToken&&) = delete;
    SingletonWithConstructToken& operator=(SingletonWithConstructToken&&) = delete;

    template <typename U = void>
        requires std::constructible_from<T, ConstructToken>
    static auto getInstance()
        -> std::shared_ptr<T>
    {
        static auto derived = std::make_shared<T>(ConstructToken { });
        return derived;
    }

    template <typename>
        requires std::constructible_from<T, ConstructToken>
    static auto getInstanceRef()
        -> T&
    {
        return *(getInstance().get());
    }
};

} // namespace X