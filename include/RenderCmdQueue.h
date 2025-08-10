/*******************************************************************************************
*
*   This data container allows adding stateful lambdas for later execution, without
*   allocating memory (other than the big memory block to store everything).
*
*   It allows the following:
*   - Stateful lambdas can be pushed
*   - Capacity grows as required (but will never decreased) 
*   - OOB (out of band) data can be pushed for things that the lambda needs to access, but
*     for whatever reason is not feasible to add to the capture list (e.g std::string) due
*     to the limitations below.
*
*   It is fast, but it has a few limitations:
*   - Captured variables need to be trivially copyable, since things are copied around
*     simply with memcpy.
*   - Due to the intended use, it is not possible to remove single elements. Once the queue
*     is processed and cleared in one go.
*
*   This example has been created using raylib 5.5 (www.raylib.com)
*   raylib is licensed under an unmodified zlib/libpng license (View raylib.h for details)
*
*   Copyright (c) 2025 Rui Figueira (https://github.com/ruifig)
*
********************************************************************************************/

#include <cstdint>
#include <type_traits>
#include <limits>
#include <assert.h>
#include <stdlib.h>
#include <string_view>

#include "raylib.h"

#if defined(_MSVC_LANG)
        __pragma(warning(push))
        __pragma(warning(disable: 5204))  /* 'type-name': class has virtual functions, but its trivial destructor is not virtual; instances of objects derived from this class may not be destructed correctly */
#endif

namespace details
{
    inline std::size_t next_pow2(std::size_t n) noexcept
    {
        std::size_t result = 1;
        while (result <= n)
        {
            result *= 2;
        }
        return result;
    }

    inline std::size_t round_pow2(std::size_t n) noexcept
    {
        if ((n == 0) || (n & (n - 1)))
            return next_pow2(n);
        return n;
    }

    /**
     * Returns `a` rounded up to a multiple of `b`
     */
    template<typename T>
    static constexpr T roundUpToMultipleOf(T a, T b)
    {
        // If `b` is 0, then we don't do any alignment
        if (b == 0)
        {
            return a;
        }

        // Integer division trick to round up `a` to a multiple of `b`
        //
        return ((a + b - 1) / b) * b;
    }
}

class RenderCmdQueue
{
  public:

    using SizeType = uint32_t;
    
    /**
     * Represents a reference to something in the container
     */
    struct Ref
    {
        Ref() = default;
        explicit Ref(SizeType pos)
            : pos(pos)
        {
        }

        inline static constexpr SizeType InvalidValue = std::numeric_limits<SizeType>::max();

        /**
         * Returns true if the iterator is set (even if pointing to end())
         * This is only useful when the user code needs to check if a reference was set to point to something or not. 
         */
        bool isSet() const noexcept
        {
            return pos != InvalidValue;
        }

        SizeType pos = InvalidValue;
    };

    /*!
     * \param capacity
     *	Initial capacity. A value of 0 is allowed.
     *	Capacity grows as required.
     */ 
    RenderCmdQueue(uint32_t capacity = 0)
    {
        m_data = static_cast<uint8_t*>(malloc(capacity));
        m_capacity = capacity;
    }

    ~RenderCmdQueue()
    {
        free(m_data);
    }

    struct Base
    {
        Base(SizeType size)
            : size (size)
        {
        }

        SizeType size;
        virtual void call(RenderCmdQueue& q) const = 0;
    };

    /*!
     * This acts as a wrapper for the lambdas, effectively enabling us to store different lambda types and
     * call their function call operator
     */
    template<typename T>
    struct Wrapper : public Base
    {
        Wrapper(T&& payload)
            : Base(sizeof(*this))
            , payload(std::forward<T>(payload))
        {
        }

        void call(RenderCmdQueue& q) const override
        {
            payload(q);
        }

        T payload;
    };

    template<typename T>
    void push(T&& v)
    {
        // T needs to be copyable with memcmp
        static_assert(std::is_trivially_copyable_v<T>);

        static constexpr size_t needed = sizeof(Wrapper<T>);

        if (getFreeCapacity() < needed)
        {
            grow(needed);
        }

        uint32_t offset = m_usedCapacity;
        [[maybe_unused]] Wrapper<T>* ptr = new(m_data + offset) Wrapper<T>(std::forward<T>(v));
        m_usedCapacity += needed;
        ++m_numElements;

        if (!m_first.isSet())
        {
            m_first = Ref(offset);
        }

        m_last = Ref(offset);
    }

    template<typename T>
    Ref oob_push_empty(size_t count)
    {
        static_assert(std::is_standard_layout_v<T>);
        static_assert(std::is_trivially_destructible_v<T>);

        SizeType alignedNeededCapacity = details::roundUpToMultipleOf(
            static_cast<SizeType>(count * sizeof(T)), static_cast<SizeType>(sizeof(size_t)));
        if (getFreeCapacity() < alignedNeededCapacity)
        {
            grow(alignedNeededCapacity);
        }

        Ref res(m_usedCapacity);
        m_usedCapacity += alignedNeededCapacity;
        if (m_last.isSet())
        {
            at(m_last).size += alignedNeededCapacity;
        }

        return res;
    }

    /*!
     * Pushes an OOB raw data.
     * OOB means "out of band", since it's data that the iterators don't see.
     * The purpose of this kind of data is for when you need to insert raw data into the vector that other objects need to use.
     */
    template<typename T>
    RenderCmdQueue::Ref oob_push(const T* data, size_t count)
    {
        Ref res = oob_push_empty(count);
        uint8_t* ptr = m_data + res.pos;
        memcpy(ptr, data, count * sizeof(T));
        return res;
    }

    /*!
     * Returns a pointer to an oob data
     */
    uint8_t* oobAt(Ref ref) const
    {
        assert(ref.pos < m_usedCapacity);
        return m_data + ref.pos;
    }

    /*!
     * Give an oob reference, it returns it's data pointer, cast to the specified type
     */
    template<typename T>
    T& oobAtAs(Ref ref) const
    {
        static_assert(std::is_standard_layout_v<T>);
        static_assert(std::is_trivially_destructible_v<T>);
        return *reinterpret_cast<T*>(oobAt(ref));
    }

    /*!
     * Runs all the commands
     */
    void callAll()
    {
        const uint8_t* ptr = m_data + (m_first.isSet() ? m_first.pos : 0);
        uint32_t todo = m_numElements;
        while (todo--)
        {
            const Base* op = reinterpret_cast<const Base*>(ptr);
            ptr += op->size;
            op->call(*this);
        }
    }

    /*!
     * Clears the queue
     */
    void clear()
    {
        m_usedCapacity = 0;
        m_numElements = 0;
        m_first = {};
        m_last = {};
    }

  private:


    /*!
     * Given a Ref, it returns the object at that position.
     */
    Base& at(Ref ref)
    {
        assert(ref.pos < m_usedCapacity);
        return *reinterpret_cast<Base*>(m_data + ref.pos);
    }

    /*!
     * Returns the free capacity, in bytes
     */
    SizeType getFreeCapacity() const
    {
        return m_capacity - m_usedCapacity;
    }

    /*!
     * Grows the container by the specified amount of bytes
     */
    void grow(SizeType requiredFreeCapacity)
    {
        SizeType newCapacity = static_cast<SizeType>(details::round_pow2(m_usedCapacity + requiredFreeCapacity));

        // Allocate new block
        uint8_t* newData = reinterpret_cast<uint8_t*>(malloc(newCapacity));

        // Copy current block to new one and adjust the header information
        if (m_data)
        {
            memcpy(newData, m_data, m_usedCapacity);
            free(m_data);
        }

        m_data = newData;
        m_capacity = newCapacity;
    }

    /*!
     * Buffer where the elements are kept
     */
    uint8_t* m_data = nullptr;

    /*!
     * Since the elements put into the container can be of different sizes, any mention of capacity therefore are in bytes.
     * not the number of elements, since that is unknown. As-in, there is no way to know how many elements will fit in the
     * available capacity.
     */
    uint32_t m_capacity = 0;
    uint32_t m_usedCapacity = 0;

    /*!
     * Number of elements in the container
     */
    uint32_t m_numElements = 0;

    /*!
     * Reference to the first inserted element and the last.
     * This is needed to support OOB data.
     */
    Ref m_first;
    Ref m_last;
};

#if defined(_MSVC_LANG)
        __pragma(warning(pop))
#endif

