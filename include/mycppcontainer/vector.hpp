#pragma once

#include<cstddef>
#include<memory>
#include<stdexcept>

namespace mycppcontainer
{
    
template<class T>
class Vector
{
private:
    T* data_{nullptr};
    std::size_t size_{0};
    std::size_t capacity_{0};

    using Alloc=std::allocator<T>;
    using Trait=std::allocator_traits<Alloc>;

    Alloc alloc;

public:
    Vector()=default;

    Vector(const Vector& v)=delete;

    Vector(Vector&& other) noexcept
        :data_(other.data_),size_(other.size_),capacity_(other.capacity_)
    {
        other.data_=nullptr;
        other.size_=0;
        other.capacity_=0;
    }

    Vector& operator=(const Vector& rhs)=delete;

    Vector& operator=(Vector&& rhs)=delete;

    ~Vector()
    {
        if(data_!=nullptr)
        {
            Trait::deallocate(alloc,data_,capacity_);
        }
    }

public:
    auto size() const noexcept { return size_; }

    auto capacity() const noexcept { return capacity_; }

    bool empty() const noexcept { return size_==0; }

    void reserve(std::size_t n)
    {
        //more than limit
        if(n>Trait::max_size(alloc))
        {
            throw std::length_error("Vector: reserve exceeds max_size");
        }

        //not enough -> allocate
        if(n>capacity_)
        {
            T* ptr=Trait::allocate(alloc,n);

            if(data_!=nullptr)
            {
                Trait::deallocate(alloc,data_,capacity_);
            }
            data_=ptr;

            capacity_=n;
        }
    }
};

}