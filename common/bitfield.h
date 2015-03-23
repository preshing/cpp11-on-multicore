//---------------------------------------------------------
// For conditions of distribution and use, see
// https://github.com/preshing/cpp11-on-multicore/blob/master/LICENSE
//---------------------------------------------------------

#ifndef __CPP11OM_BITFIELD_H__
#define __CPP11OM_BITFIELD_H__

#include <cassert>


//---------------------------------------------------------
// BitFieldMember<>: Used internally by ADD_BITFIELD_MEMBER macro.
//---------------------------------------------------------
template <class T, int Offset, int Bits>
class BitFieldMember
{
private:
    static_assert(Offset + Bits <= (int) sizeof(T) * 8, "Member exceeds bitfield boundaries");
    static_assert(Bits < (int) sizeof(T) * 8, "Can't fill entire bitfield with one member");

    static const T Maximum = (T(1) << Bits) - 1;
    static const T Mask = Maximum << Offset;

    T value;

public:
    T maximum() const { return Maximum; }
    T one() const { return T(1) << Offset; }

    operator T() const
    {
        return (value >> Offset) & Maximum;
    }

    BitFieldMember& operator=(T v)
    {
        assert(v <= Maximum);               // v must fit inside the bitfield member
        value = (value & ~Mask) | (v << Offset);
        return *this;
    }

    BitFieldMember& operator+=(T v)
    {
        assert(T(*this) + v <= Maximum);    // result must fit inside the bitfield member
        value += v << Offset;
        return *this;
    }

    BitFieldMember& operator-=(T v)
    {
        assert(T(*this) >= v);              // result must not underflow
        value -= v << Offset;
        return *this;
    }

    BitFieldMember& operator++() { return *this += 1; }
    BitFieldMember& operator++(int) { return *this += 1; }    // postfix form
    BitFieldMember& operator--() { return *this -= 1; }
    BitFieldMember& operator--(int) { return *this -= 1; }    // postfix form
};


//---------------------------------------------------------
// BitFieldArray<>: Used internally by ADD_BITFIELD_ARRAY macro.
//---------------------------------------------------------
template <class T, int BaseOffset, int BitsPerItem, int NumItems>
class BitFieldArray
{
private:
    static_assert(BaseOffset + BitsPerItem * NumItems <= (int) sizeof(T) * 8, "Array exceeds bitfield boundaries");
    static_assert(BitsPerItem < (int) sizeof(T) * 8, "Can't fill entire bitfield with one array element");

    static const T Maximum = (T(1) << BitsPerItem) - 1;

    T value;

    class Element
    {
    private:
        T& value;
        int offset;

    public:
        Element(T& value, int offset) : value(value), offset(offset) {}
        T mask() const { return Maximum << offset; }

        operator T() const
        {
            return (value >> offset) & Maximum;
        }

        Element& operator=(T v)
        {
            assert(v <= Maximum);               // v must fit inside the bitfield member
            value = (value & ~mask()) | (v << offset);
            return *this;
        }

        Element& operator+=(T v)
        {
            assert(T(*this) + v <= Maximum);    // result must fit inside the bitfield member
            value += v << offset;
            return *this;
        }

        Element& operator-=(T v)
        {
            assert(T(*this) >= v);               // result must not underflow
            value -= v << offset;
            return *this;
        }

        Element& operator++() { return *this += 1; }
        Element& operator++(int) { return *this += 1; }    // postfix form
        Element& operator--() { return *this -= 1; }
        Element& operator--(int) { return *this -= 1; }    // postfix form
    };

public:
    T maximum() const { return Maximum; }
    int numItems() const { return NumItems; }

    Element operator[](int i)
    {
        assert(i >= 0 && i < NumItems);     // array index must be in range
        return Element(value, BaseOffset + BitsPerItem * i);
    }

    const Element operator[](int i) const
    {
        assert(i >= 0 && i < NumItems);     // array index must be in range
        return Element(value, BaseOffset + BitsPerItem * i);
    }
};


//---------------------------------------------------------
// Bitfield definition macros.
// For usage examples, see RWLock and LockReducedDiningPhilosophers.
//---------------------------------------------------------
#define BEGIN_BITFIELD_TYPE(typeName, T) \
    union typeName \
    { \
    private: \
        typedef T _IntType; \
        T value; \
    public: \
        typeName(T v = 0) : value(v) {} \
        typeName& operator=(T v) { value = v; return *this; } \
        operator T&() { return value; } \
        operator T() const { return value; }

#define ADD_BITFIELD_MEMBER(memberName, offset, bits) \
        BitFieldMember<_IntType, offset, bits> memberName;

#define ADD_BITFIELD_ARRAY(memberName, offset, bits, numItems) \
        BitFieldArray<_IntType, offset, bits, numItems> memberName;

#define END_BITFIELD_TYPE() \
    };


#endif // __CPP11OM_BITFIELD_H__
