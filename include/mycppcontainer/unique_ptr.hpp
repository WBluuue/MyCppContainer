#pragma once

namespace mycppcontainer
{

template<typename T>
class UniquePtr
{
private:
    T* data_{nullptr};
public:
    UniquePtr() noexcept {}

    explicit UniquePtr(T* ptr) noexcept
    {
        if(ptr!=nullptr)
        {
            data_=ptr;
        }
    }

    ~UniquePtr() noexcept
    {
        delete data_;
    }

    UniquePtr(const UniquePtr&)=delete;

    UniquePtr(UniquePtr&& other)=delete;

    UniquePtr& operator=(const UniquePtr&)=delete;

    UniquePtr& operator=(UniquePtr&& rhs)=delete;

public:
    T* get() const noexcept { return data_; }

    T& operator*() const noexcept { return *data_; }

    T* operator->() const noexcept { return data_; };

    explicit operator bool() const noexcept { return data_!=nullptr; }
};

}
