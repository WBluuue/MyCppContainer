#pragma once

#include<cstddef>
#include<memory>
#include<stdexcept>
#include<utility>

namespace mycppcontainer
{
    
template<typename T>
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

    Vector& operator=(Vector&& rhs) noexcept
    {
        if(this!=(&rhs))
        {
            for(std::size_t i=0;i<size_;++i)
            {
                Trait::destroy(alloc,data_+i);
            }
            if(data_!=nullptr)
            {
                Trait::deallocate(alloc,data_,capacity_);
            }

            data_=rhs.data_;
            size_=rhs.size_;
            capacity_=rhs.capacity_;

            rhs.data_=nullptr;
            rhs.size_=0;
            rhs.capacity_=0;
        }
        return *this;
    }

    ~Vector()
    {
        for(std::size_t i=0;i<size_;++i)
        {
            Trait::destroy(alloc,data_+i);
        }
        if(data_!=nullptr)
        {
            Trait::deallocate(alloc,data_,capacity_);
        }
    }

public:
    auto size() const noexcept { return size_; }

    auto capacity() const noexcept { return capacity_; }

    bool empty() const noexcept { return size_==0; }

public:
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

            for(std::size_t i=0;i<size_;++i)
            {
                try
                {
                    Trait::construct(alloc,ptr+i,std::move_if_noexcept(*(data_+i)));   
                }
                catch(...)
                {
                    for(std::size_t j=0;j<i;j++)
                    {
                        Trait::destroy(alloc,ptr+j);
                    }
                    Trait::deallocate(alloc,ptr,n);
                    throw;   
                }
            }
             
            for(std::size_t i=0;i<size_;++i)
            {
                Trait::destroy(alloc,data_+i);
            }
            if(data_!=nullptr)
            {
                Trait::deallocate(alloc,data_,capacity_);
            }

            data_=ptr;
            capacity_=n;
        }
    }

    void push_back(const T& value)
    {
        if(size_==capacity_)
        {
            throw std::length_error("Vector: push_back requires reserved capacity");
        }

        Trait::construct(alloc,data_+size_,value);
        size_++;
    }

    void push_back(T&& value)
    {
        if(size_==capacity_)
        {
            throw std::length_error("Vector: push_back requires reserved capacity");
        }

        Trait::construct(alloc,data_+size_,std::move(value));
        size_++;
    }

    T& operator[](std::size_t i) noexcept
    {
        return *(data_+i);
    }

    const T& operator[](std::size_t i) const noexcept
    {
        return *(data_+i);
    }

    void clear() noexcept
    {
        for(std::size_t i=0;i<size_;++i)
        {
            Trait::destroy(alloc,data_+i);
        }
        size_=0;
    }

    template<typename... Args>
    T& emplace_back(Args&&... args)
    {
        if(size_==capacity_)
        {
            throw std::length_error("Vector: emplace_back requires reserved capacity");
        }

        Trait::construct(alloc,data_+size_,std::forward<Args>(args)...);
        size_++;
        return *(data_+size_-1);
    }
};

}