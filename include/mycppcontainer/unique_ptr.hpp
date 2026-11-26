#pragma once

#include<memory>
#include<utility>
#include<type_traits>

namespace mycppcontainer
{

template<typename T,typename Deleter=std::default_delete<T>>
class UniquePtr
{
private:
    T* data_{nullptr};
    [[no_unique_address]] Deleter deleter_{};
public:
    UniquePtr() noexcept 
        requires std::is_nothrow_default_constructible_v<Deleter>
    {}

    explicit UniquePtr(T* ptr) noexcept
        requires std::is_nothrow_default_constructible_v<Deleter>
    :   data_(ptr)
    {}

    explicit UniquePtr(T* ptr,Deleter deleter) noexcept
    :   data_(ptr),deleter_(std::move(deleter))
    {}

    ~UniquePtr() noexcept
    {
        if(data_!=nullptr)
        {
            deleter_(data_);
        }
    }

    UniquePtr(const UniquePtr&)=delete;

    UniquePtr(UniquePtr&& other) noexcept
    : data_(other.data_),deleter_(std::move(other.deleter_))
    {
        other.data_=nullptr;
    }

    UniquePtr& operator=(const UniquePtr&)=delete;

    UniquePtr& operator=(UniquePtr&& rhs) noexcept
    {
        if(this!=(&rhs))
        {
            if(data_!=nullptr)
            {
                deleter_(data_);
            }

            data_=rhs.data_;
            rhs.data_=nullptr;

            deleter_=std::move(rhs.deleter_);
        }
        return *this;   
    }

public:
    T* get() const noexcept { return data_; }

    Deleter& get_deleter() noexcept { return deleter_; }

    const Deleter& get_deleter() const noexcept { return deleter_; }

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

        if(optr!=nullptr)
        {
            deleter_(optr);
        }
    }

    void swap(UniquePtr& other) noexcept
    {
        if(this!=(&other))
        {
            std::swap(data_,other.data_);
            std::swap(deleter_,other.deleter_);
        }
    }
};

}
