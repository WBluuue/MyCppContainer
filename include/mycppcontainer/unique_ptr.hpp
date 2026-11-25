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

public:
    T* release() noexcept
    {
        auto ptr=data_;
        data_=nullptr;
        return ptr;
    }

    void reset(T* ptr=nullptr) noexcept
    {
        auto optr=data_;
        data_=ptr;
        delete optr;
    }

    void swap(UniquePtr& other) noexcept
    {
        auto optr=other.data_;
        other.data_=data_;
        data_=optr;
    }
};

}
