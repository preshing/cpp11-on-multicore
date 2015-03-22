//---------------------------------------------------------
// For conditions of distribution and use, see
// https://github.com/preshing/cpp11-on-multicore/blob/master/LICENSE
//---------------------------------------------------------

#ifndef __CPP11OM_BITFIELD_H__
#define __CPP11OM_BITFIELD_H__

#include <cassert>


//---------------------------------------------------------
// BitField<>: Used internally by BEGIN_BITFIELD_TYPE macro.
// T is expected to be an unsigned integral type.
//---------------------------------------------------------
template <class T>
struct BitField
{
    typedef T IntType;
    T value;

    BitField(T v = 0) : value(v) {}
    BitField& baseCast() { return *this; }
    const BitField& baseCast() const { return *this; }
};


//---------------------------------------------------------
// BitFieldMember<>: Used internally by ADD_BITFIELD_MEMBER macro.
//---------------------------------------------------------
template <class T, int Offset, int Bits>
struct BitFieldMember : BitField<T>
{
    static_assert(Offset + Bits <= (int) sizeof(T) * 8, "Member exceeds bitfield boundaries");
    static_assert(Bits < (int) sizeof(T) * 8, "Can't fill entire bitfield with one member");

    // Compile-time constants.
    // Not easy to access since the template instance is wrapped in a macro.
    // You can get at them using eg. BitFieldType().member().maximum() instead.
    static const T Maximum = (T(1) << Bits) - 1;
    static const T Mask = Maximum << Offset;

    // Can't instantiate this class directly.
    // Must instead cast from BitField<T>.
    // That's what ADD_BITFIELD_MEMBER does.
    BitFieldMember() = delete;

    T maximum() const { return Maximum; }
    T one() const { return T(1) << Offset; }

    T get() const
    {
        return (this->value >> Offset) & Maximum;
    }

    void set(T v)
    {
        assert(v <= Maximum);   // v must fit inside the bitfield member
        this->value = (this->value & ~Mask) | (v << Offset);
    }

    void setWrapped(T v)
    {
        this->value = (this->value & ~Mask) | ((v & Maximum) << Offset);
    }

    void add(T v)
    {
        assert(get() + v <= Maximum);   // result must fit inside the bitfield member
        this->value += v << Offset;
    }

    void addWrapped(T v)
    {
        this->value = (this->value & ~Mask) | ((this->value + (v << Offset)) & Mask);
    }

    void sub(T v)
    {
        assert(get() >= v);     // result must not underflow
        this->value -= v << Offset;
    }

    void subWrapped(T v)
    {
        this->value = (this->value & ~Mask) | ((this->value - (v << Offset)) & Mask);
    }
};


//---------------------------------------------------------
// BitFieldArray<>: Used internally by ADD_BITFIELD_ARRAY macro.
//---------------------------------------------------------
template <class T, int BaseOffset, int BitsPerItem, int NumItems>
struct BitFieldArray : BitField<T>
{
    static_assert(BaseOffset + BitsPerItem * NumItems <= (int) sizeof(T) * 8, "Array exceeds bitfield boundaries");
    static_assert(BitsPerItem < (int) sizeof(T) * 8, "Can't fill entire bitfield with one array element");

    // Compile-time constants.
    // Not easy to access since the template instance is wrapped in a macro.
    // You can get at them using eg. BitFieldType().member().maximum() instead.
    static const T Maximum = (T(1) << BitsPerItem) - 1;

    // Can't instantiate this class directly.
    // Must instead cast from BitField<T>.
    // That's what ADD_BITFIELD_ARRAY does.
    BitFieldArray() = delete;

    T maximum() const { return Maximum; }
    int offset(int i) const
    {
        assert(i >= 0 && i < NumItems);     // array index must be in range
        return BaseOffset + BitsPerItem * i;
    }
    T one(int i) const { return T(1) << offset(i); }
    T mask(int i) const { return Maximum << offset(i); }
    int numItems() const { return NumItems; }

    T get(int i) const
    {
        return (this->value >> offset(i)) & Maximum;
    }

    void set(int i, T v)
    {
        assert(v <= Maximum);   // v must fit inside the bitfield member
        this->value = (this->value & ~mask(i)) | (v << offset(i));
    }

    void setWrapped(int i, T v)
    {
        this->value = (this->value & ~mask(i)) | ((v & Maximum) << offset(i));
    }

    void add(int i, T v)
    {
        assert(get(i) + v <= Maximum);   // result must fit inside the bitfield member
        this->value += v << offset(i);
    }

    void addWrapped(int i, T v)
    {
        T m = mask(i);
        this->value = (this->value & ~m) | ((this->value + (v << offset(i))) & m);
    }

    void sub(int i, T v)
    {
        assert(get(i) >= v);    // result must not underflow
        this->value -= v << offset(i);
    }

    void subWrapped(int i, T v)
    {
        T m = mask(i);
        this->value = (this->value & ~m) | ((this->value - (v << offset(i))) & m);
    }
};


//---------------------------------------------------------
// Bitfield definition macros.
// For usage examples, see RWLock and LockReducedDiningPhilosophers.
//---------------------------------------------------------
#define BEGIN_BITFIELD_TYPE(typeName, T) \
    struct typeName : BitField<T> \
    { \
        typeName(T v = 0) : BitField(v) {}

#define ADD_BITFIELD_MEMBER(memberName, offset, bits) \
        BitFieldMember<IntType, offset, bits>& memberName() { return static_cast<BitFieldMember<IntType, offset, bits>&>(baseCast()); } \
        const BitFieldMember<IntType, offset, bits>& memberName() const { return static_cast<const BitFieldMember<IntType, offset, bits>&>(baseCast()); }

#define ADD_BITFIELD_ARRAY(memberName, offset, bitsPerItem, numItems) \
        BitFieldArray<IntType, offset, bitsPerItem, numItems>& memberName() { return static_cast<BitFieldArray<IntType, offset, bitsPerItem, numItems>&>(baseCast()); } \
        const BitFieldArray<IntType, offset, bitsPerItem, numItems>& memberName() const { return static_cast<const BitFieldArray<IntType, offset, bitsPerItem, numItems>&>(baseCast()); }

#define END_BITFIELD_TYPE() \
    };


#endif // __CPP11OM_BITFIELD_H__
