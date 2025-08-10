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
    inline std::size_t NextPow2(std::size_t n) noexcept
    {
        std::size_t result = 1;
        while (result <= n)
        {
            result *= 2;
        }
        return result;
    }

    inline std::size_t RoundPow2(std::size_t n) noexcept
    {
        if ((n == 0) || (n & (n - 1)))
            return NextPow2(n);
        return n;
    }

    /**
     * Returns `a` rounded up to a multiple of `b`
     */
    template<typename T>
    static constexpr T RoundUpToMultipleOf(T a, T b)
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

} // namespace details


/*!
 * Data container for for trivially copyable lambdas.
 */
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
            : Pos(pos)
        {
        }

        inline static constexpr SizeType InvalidValue = std::numeric_limits<SizeType>::max();

        /**
         * Returns true if the iterator is set (even if pointing to end())
         * This is only useful when the user code needs to check if a reference was set to point to something or not. 
         */
        bool IsSet() const noexcept
        {
            return Pos != InvalidValue;
        }

        SizeType Pos = InvalidValue;
    };

    /*!
     * \param capacity
     *	Initial capacity. A value of 0 is allowed.
     *	Capacity grows as required.
     */ 
    RenderCmdQueue(uint32_t capacity = 0)
    {
        Data = static_cast<uint8_t*>(malloc(capacity));
        Capacity = capacity;
    }

    ~RenderCmdQueue()
    {
        free(Data);
    }

    struct Base
    {
        Base(SizeType size)
            : Size (size)
        {
        }

        SizeType Size;
        virtual void Call(RenderCmdQueue& q) const = 0;
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
            , Payload(std::forward<T>(payload))
        {
        }

        void Call(RenderCmdQueue& q) const override
        {
            Payload(q);
        }

        T Payload;
    };

    template<typename T>
    void Push(T&& v)
    {
        // T needs to be copyable with memcmp
        static_assert(std::is_trivially_copyable_v<T>);

        static constexpr size_t needed = sizeof(Wrapper<T>);

        if (GetFreeCapacity() < needed)
        {
            Grow(needed);
        }

        uint32_t offset = UsedCapacity;
        [[maybe_unused]] Wrapper<T>* ptr = new(Data + offset) Wrapper<T>(std::forward<T>(v));
        UsedCapacity += needed;
        ++NumElements;

        if (!First.IsSet())
        {
            First = Ref(offset);
        }

        Last = Ref(offset);
    }

    template<typename T>
    Ref OobPushEmpty(size_t count)
    {
        static_assert(std::is_trivially_copyable_v<T>);

        SizeType alignedNeededCapacity = details::RoundUpToMultipleOf(
            static_cast<SizeType>(count * sizeof(T)), static_cast<SizeType>(sizeof(size_t)));
        if (GetFreeCapacity() < alignedNeededCapacity)
        {
            Grow(alignedNeededCapacity);
        }

        Ref res(UsedCapacity);
        UsedCapacity += alignedNeededCapacity;
        if (Last.IsSet())
        {
            At(Last).Size += alignedNeededCapacity;
        }

        return res;
    }

    /*!
     * Pushes an OOB raw data.
     * OOB means "out of band", since it's data that the iterators don't see.
     * The purpose of this kind of data is for when you need to insert raw data into the vector that other objects need to use.
     */
    template<typename T>
    RenderCmdQueue::Ref OobPush(const T* data, size_t count)
    {
        Ref res = OobPushEmpty(count);
        uint8_t* ptr = Data + res.Pos;
        memcpy(ptr, data, count * sizeof(T));
        return res;
    }

    /*!
     * Returns a pointer to an oob data
     */
    uint8_t* OobAt(Ref ref) const
    {
        assert(ref.Pos < UsedCapacity);
        return Data + ref.Pos;
    }

    /*!
     * Give an oob reference, it returns it's data pointer, cast to the specified type
     */
    template<typename T>
    T& OobAtAs(Ref ref) const
    {
        static_assert(std::is_trivially_copyable_v<T>);
        return *reinterpret_cast<T*>(OobAt(ref));
    }

    /*!
     * Runs all the commands
     */
    void CallAll()
    {
        const uint8_t* ptr = Data + (First.IsSet() ? First.Pos : 0);
        uint32_t todo = NumElements;
        while (todo--)
        {
            const Base* op = reinterpret_cast<const Base*>(ptr);
            ptr += op->Size;
            op->Call(*this);
        }
    }

    /*!
     * Clears the queue
     */
    void Clear()
    {
        UsedCapacity = 0;
        NumElements = 0;
        First = {};
        Last = {};
    }

  private:


    /*!
     * Given a Ref, it returns the object at that position.
     */
    Base& At(Ref ref)
    {
        assert(ref.Pos < UsedCapacity);
        return *reinterpret_cast<Base*>(Data + ref.Pos);
    }

    /*!
     * Returns the free capacity, in bytes
     */
    SizeType GetFreeCapacity() const
    {
        return Capacity - UsedCapacity;
    }

    /*!
     * Grows the container by the specified amount of bytes
     */
    void Grow(SizeType requiredFreeCapacity)
    {
        SizeType newCapacity = static_cast<SizeType>(details::RoundPow2(UsedCapacity + requiredFreeCapacity));

        // Allocate new block
        uint8_t* newData = reinterpret_cast<uint8_t*>(malloc(newCapacity));

        // Copy current block to new one and adjust the header information
        if (Data)
        {
            memcpy(newData, Data, UsedCapacity);
            free(Data);
        }

        Data = newData;
        Capacity = newCapacity;
    }

    /*!
     * Buffer where the elements are kept
     */
    uint8_t* Data = nullptr;

    /*!
     * Since the elements put into the container can be of different sizes, any mention of capacity therefore are in bytes.
     * not the number of elements, since that is unknown. As-in, there is no way to know how many elements will fit in the
     * available capacity.
     */
    uint32_t Capacity = 0;
    uint32_t UsedCapacity = 0;

    /*!
     * Number of elements in the container
     */
    uint32_t NumElements = 0;

    /*!
     * Reference to the first inserted element and the last.
     * This is needed to support OOB data.
     */
    Ref First;
    Ref Last;
};

#if defined(_MSVC_LANG)
        __pragma(warning(pop))
#endif

