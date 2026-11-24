#pragma once

#include<cstddef>
#include<memory>
#include<stdexcept>
#include<utility>
#include<type_traits>

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

    Vector(const Vector& other)
        requires std::is_copy_constructible_v<T>
    {
        if(other.empty())
        {
            return ;
        }

        data_=Trait::allocate(alloc,other.size_);
        for(std::size_t i=0;i<other.size_;++i)
        {
            try
            {
                Trait::construct(alloc,data_+i,other[i]);
            }
            catch(...)
            {
                for(std::size_t j=0;j<i;++j)
                {
                    Trait::destroy(alloc,data_+j);
                }
                Trait::deallocate(alloc,data_,other.size_);
                data_=nullptr;
                throw;
            }
        }
        size_=other.size_;
        capacity_=other.size_;
    }

    Vector(Vector&& other) noexcept
        :data_(other.data_),size_(other.size_),capacity_(other.capacity_)
    {
        other.data_=nullptr;
        other.size_=0;
        other.capacity_=0;
    }

    Vector& operator=(const Vector& rhs)
        requires std::is_copy_constructible_v<T>
    {
        if(this!=(&rhs))
        {
            auto newVector(rhs);
            *this=std::move(newVector);
        }
        return *this;
    }

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
    std::size_t size() const noexcept { return size_; }

    std::size_t capacity() const noexcept { return capacity_; }

    T* data() noexcept { return data_; }

    const T* data() const noexcept { return data_; }

    bool empty() const noexcept { return size_==0; }

    T& operator[](std::size_t i) noexcept
    {
        return *(data_+i);
    }

    const T& operator[](std::size_t i) const noexcept
    {
        return *(data_+i);
    }

    T& front() noexcept
    {
        return *data_;
    }

    const T& front() const noexcept
    {
        return *data_;
    }

    T& back() noexcept
    {
        return *(data_+size_-1);
    }

    const T& back() const noexcept
    {
        return *(data_+size_-1);
    }

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
            T* newData_=Trait::allocate(alloc,n);

            for(std::size_t i=0;i<size_;++i)
            {
                try
                {
                    Trait::construct(alloc,newData_+i,std::move_if_noexcept(*(data_+i)));   
                }
                catch(...)
                {
                    for(std::size_t j=0;j<i;j++)
                    {
                        Trait::destroy(alloc,newData_+j);
                    }
                    Trait::deallocate(alloc,newData_,n);
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

            data_=newData_;
            capacity_=n;
        }
    }

    void push_back(const T& value)
    {
        this->emplace_back(value);
    }

    void push_back(T&& value)
    {
        this->emplace_back(std::move(value));
    }

    template<typename... Args>
    T& emplace_back(Args&&... args)
    {
        if(size_==capacity_)
        {
            std::size_t limit=Trait::max_size(alloc);
            if(capacity_==limit)
            {
                throw std::length_error("Vector: reserve exceeds max_size");
            }

            auto newCapacity_=(capacity_<=(limit>>1)?(capacity_==0?1:capacity_<<1):limit);

            auto newData_=Trait::allocate(alloc,newCapacity_);
            try
            {
                Trait::construct(alloc,newData_+size_,std::forward<Args>(args)...);
            }
            catch(...)
            {
                Trait::deallocate(alloc,newData_,newCapacity_);
                throw;
            }
            
            for(std::size_t i=0;i<size_;++i)
            {
                try
                {
                    Trait::construct(alloc,newData_+i,std::move_if_noexcept(*(data_+i)));   
                }
                catch(...)
                {
                    for(std::size_t j=0;j<i;++j)
                    {
                        Trait::destroy(alloc,newData_+j);
                    }
                    Trait::destroy(alloc,newData_+size_);
                    Trait::deallocate(alloc,newData_,newCapacity_);
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

            data_=newData_;
            capacity_=newCapacity_;
            size_++;
            return *(data_+size_-1);
        }

        Trait::construct(alloc,data_+size_,std::forward<Args>(args)...);
        size_++;
        return *(data_+size_-1);
    }

    void pop_back() noexcept
    {
        size_--;
        Trait::destroy(alloc,data_+size_);   
    }

    void clear() noexcept
    {
        for(std::size_t i=0;i<size_;++i)
        {
            Trait::destroy(alloc,data_+i);
        }
        size_=0;
    }
};

}