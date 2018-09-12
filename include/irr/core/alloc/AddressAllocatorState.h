// Copyright (C) 2018 Mateusz 'DevSH' Kielan
// This file is part of the "IrrlichtBAW Engine"
// For conditions of distribution and use, see copyright notice in irrlicht.h

#ifndef __IRR_ADDRESS_ALLOCATOR_STATE_H_INCLUDED__
#define __IRR_ADDRESS_ALLOCATOR_STATE_H_INCLUDED__

#include "IrrCompileConfig.h"

namespace irr
{
namespace core
{

template<class AddressAlloc>
class AddressAllocatorState : public AddressAlloc, public IReferenceCounted
{
        const typename AddressAlloc::size_type  reservedSpace;
        uint8_t* const                          bufferStart;
    protected:
        virtual ~AddressAllocatorState() {}
    public:

#define CALC_RESERVED_SPACE
        template<typename... Args>
        AddressAllocatorState(void* buffer, typename AddressAlloc::size_type bufSz, Args&&... args) noexcept :
                AddressAlloc(buffer, reinterpret_cast<typename AddressAlloc::size_type>(reinterpret_cast<uint8_t*>(buffer)+AddressAlloc::reserved_size(reinterpret_cast<size_t>(buffer),bufSz,std::forward<Args>(args)...)), bufSz-AddressAlloc::reserved_size(reinterpret_cast<size_t>(buffer),bufSz,std::forward<Args>(args)...), std::forward<Args>(args)...),
                        reservedSpace(AddressAlloc::reserved_size(reinterpret_cast<size_t>(buffer),bufSz,std::forward<Args>(args)...)),
                        bufferStart(reinterpret_cast<uint8_t*>(buffer)+reservedSpace)
        {
        }
#undef CALC_RESERVED_SPACE

        inline uint8_t*                         getBufferStart() noexcept {return bufferStart;}
};


//! B is an IBuffer derived type, S is some AddressAllocatorState
template<class M, class S>
class AllocatorStateDriverMemoryAdaptor : public S
{
        M* const    memory;
    protected:
        virtual ~AllocatorStateDriverMemoryAdaptor()
        {
            if (memory) // move constructor compatibility
                memory->drop();
        }
    public:
        template<typename... Args>
        AllocatorStateDriverMemoryAdaptor(M* mem, Args&&... args) noexcept :
                    S(mem->getMappedPointer(),mem->getAllocationSize(),std::forward<Args>(args)...), memory(mem)
        {
            memory->grab();
        }
};

}
}

#endif // __IRR_ADDRESS_ALLOCATOR_STATE_H_INCLUDED__

