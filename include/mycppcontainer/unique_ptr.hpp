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

    UniquePtr(UniquePtr&& other) noexcept
    {
        data_=other.data_;
        other.data_=nullptr;   
    }

    UniquePtr& operator=(const UniquePtr&)=delete;

    UniquePtr& operator=(UniquePtr&& rhs) noexcept
    {
        if(this!=(&rhs))
        {
            delete data_;

            data_=rhs.data_;
            rhs.data_=nullptr;
        }
        return *this;   
    }

public:
    T* get() const noexcept { return data_; }

    T& operator*() const noexcept { return *data_; }

    T* operator->() const noexcept { return data_; };

    explicit operator bool() const noexcept { return data_!=nullptr; }
};

}
