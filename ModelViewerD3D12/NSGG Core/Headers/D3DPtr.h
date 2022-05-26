#pragma once

#include <Unknwn.h>

template<typename T>
class D3DPtr
{
private:
    IUnknown* ptr = nullptr;

public:
    D3DPtr() : ptr(nullptr)
    {
        // EMPTY
    }

    D3DPtr(const D3DPtr& other) = delete;
    D3DPtr& operator=(const D3DPtr& other) = delete;

    D3DPtr(D3DPtr&& other) noexcept
    {
        this->ptr = other.ptr;
        other.ptr = nullptr;
    }

    D3DPtr& operator=(D3DPtr&& other) noexcept
    {
        if (this != std::addressof(other))
        {
            if (ptr != nullptr)
                ptr->Release();

            this->ptr = other.ptr;
            other.ptr = nullptr;
        }

        return *this;
    }

    D3DPtr(T* toStore) : ptr(toStore)
    {
        //EMPTY
    }

    D3DPtr(IUnknown* toStore) : ptr(toStore)
    {
        //EMPTY
    }

    ~D3DPtr()
    {
        if (ptr != nullptr)
            ptr->Release();
    }

    T* operator->()
    {
        return static_cast<T*>(ptr);
    }

    operator T*()
    {
        return static_cast<T*>(ptr);
    }

    T** operator&()
    {
        return reinterpret_cast<T**>(&ptr);
    }

    T* Get()
    {
        return static_cast<T*>(ptr);
    }
};