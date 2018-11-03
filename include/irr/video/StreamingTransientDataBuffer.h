#ifndef __IRR_STREAMING_TRANSIENT_DATA_BUFFER_H__
#define __IRR_STREAMING_TRANSIENT_DATA_BUFFER_H__


#include "irr/core/IReferenceCounted.h"
#include "irr/core/alloc/GeneralpurposeAddressAllocator.h"
#include "irr/core/alloc/HeterogenousMemoryAddressAllocatorAdaptor.h"
#include "irr/video/StreamingGPUBufferAllocator.h"

namespace irr
{
namespace video
{


template< typename _size_type=uint32_t, class CPUAllocator=core::allocator<uint8_t> >
class StreamingTransientDataBufferST : protected core::impl::FriendOfHeterogenousMemoryAddressAllocatorAdaptor, public virtual core::IReferenceCounted
{
    protected:
        typedef core::GeneralpurposeAddressAllocator<_size_type>                                                        BasicAddressAllocator;
        core::HeterogenousMemoryAddressAllocatorAdaptor<BasicAddressAllocator,StreamingGPUBufferAllocator,CPUAllocator> mAllocator; // no point for a streaming buffer to grow
    public:
        typedef typename BasicAddressAllocator::size_type           size_type;
        static constexpr size_type                                  invalid_address = BasicAddressAllocator::invalid_address;

        //!
        /**
        \param default minAllocSize has been carefully picked to reflect the lowest nonCoherentAtomSize under Vulkan 1.1 which is not 1u .*/
        StreamingTransientDataBufferST(IVideoDriver* inDriver, const IDriverMemoryBacked::SDriverMemoryRequirements& bufferReqs,
                                       const CPUAllocator& reservedMemAllocator=CPUAllocator(), size_type minAllocSize=64u) :
                                mAllocator(reservedMemAllocator,StreamingGPUBufferAllocator(inDriver,bufferReqs),bufferReqs.vulkanReqs.size,minAllocSize)
        {
        }

        template<typename... Args>
        StreamingTransientDataBufferST(IVideoDriver* inDriver, const IDriverMemoryBacked::SDriverMemoryRequirements& bufferReqs,
                                       const CPUAllocator& reservedMemAllocator=CPUAllocator(), Args&&... args) :
                                mAllocator(reservedMemAllocator,StreamingGPUBufferAllocator(inDriver,bufferReqs),bufferReqs.vulkanReqs.size,std::forward<Args>(args)...)
        {
        }

        virtual ~StreamingTransientDataBufferST() {}


        inline bool         needsManualFlushOrInvalidate() const {return !(getBuffer()->getMemoryReqs().mappingCapability&video::IDriverMemoryAllocation::EMCF_COHERENT);}

        inline IGPUBuffer*  getBuffer() noexcept {return core::impl::FriendOfHeterogenousMemoryAddressAllocatorAdaptor::getDataAllocator(mAllocator).getAllocatedBuffer();}

        inline void*        getBufferPointer() noexcept {return core::impl::FriendOfHeterogenousMemoryAddressAllocatorAdaptor::getDataAllocator(mAllocator).getAllocatedPointer();}

        // have to try each fence once (need a function for that)
        // but only try a few fences before the next alloc attempt
        // hpw many fences to try at each step?
        template<typename... Args>
        inline size_type    multi_alloc(uint32_t count, size_type* outAddresses, const size_type* bytes, const Args&... args) noexcept
        {
            // try allocate once
            size_type unallocatedSize = try_multi_alloc(count,outAddresses,bytes,args...);
            if (!unallocatedSize)
                return 0u;

            auto maxWaitPoint = std::chrono::high_resolution_clock::now()+std::chrono::nanoseconds(50000ull); // 50 us
            // then try to wait at least once and allocate
            do
            {
                deferredFrees.waitUntilForReadyEvents(maxWaitPoint,unallocatedSize);

                unallocatedSize = try_multi_alloc(count,outAddresses,bytes,args...);
                if (!unallocatedSize)
                    return 0u;
            } while(std::chrono::high_resolution_clock::now()<maxWaitPoint);

            return unallocatedSize;
        }

        template<typename... Args>
        inline size_type    multi_place(uint32_t count, const void* const* dataToPlace, size_type* outAddresses, const size_type* bytes, const size_type* alignment, const Args&... args) noexcept
        {
        #ifdef _DEBUG
            assert(getBuffer()->getBoundMemory());
        #endif // _DEBUG
            auto retval = multi_alloc(count,outAddresses,bytes,alignment,args...);
            // fill with data
            for (uint32_t i=0; i<count; i++)
            {
                if (outAddresses[i]!=invalid_address)
                    memcpy(reinterpret_cast<uint8_t*>(getBufferPointer())+outAddresses[i],dataToPlace[i],bytes[i]);
            }
            return retval;
        }

        inline void         multi_free(uint32_t count, const size_type* addr, const size_type* bytes, IDriverFence* fence) noexcept
        {
            if (fence)
                deferredFrees.addEvent(GPUEventWrapper(fence),DeferredFreeFunctor(&mAllocator,count,addr,bytes));
            else
                mAllocator.multi_free_addr(count,addr,bytes);
        }
    protected:
        template<typename... Args>
        inline size_type    try_multi_alloc(uint32_t count, size_type* outAddresses, const size_type* bytes, const Args&... args) noexcept
        {
            mAllocator.multi_alloc_addr(count,outAddresses,bytes,args...);

            size_type unallocatedSize = 0;
            for (uint32_t i=0u; i<count; i++)
            {
                if (outAddresses[i]!=invalid_address)
                    continue;

                unallocatedSize += bytes[i];
            }
            return unallocatedSize;
        }

        class DeferredFreeFunctor : protected core::impl::FriendOfHeterogenousMemoryAddressAllocatorAdaptor
        {
            public:
                DeferredFreeFunctor(core::HeterogenousMemoryAddressAllocatorAdaptor<BasicAddressAllocator,StreamingGPUBufferAllocator,CPUAllocator>* alloctr,
                                    size_type numAllocsToFree, const size_type* addrs, const size_type* bytes) : allocRef(alloctr), rangeData(nullptr), numAllocs(numAllocsToFree)
                {
                    rangeData = reinterpret_cast<size_type*>(getHostAllocator(*allocRef).allocate(sizeof(size_type)*numAllocs*2u,sizeof(size_type)));
                    memcpy(rangeData            ,addrs,sizeof(size_type)*numAllocs);
                    memcpy(rangeData+numAllocs  ,bytes,sizeof(size_type)*numAllocs);
                }
                DeferredFreeFunctor(DeferredFreeFunctor&& other)
                {
                    *this = std::forward<DeferredFreeFunctor>(other);
                }
                DeferredFreeFunctor(const DeferredFreeFunctor& other) = delete;

                ~DeferredFreeFunctor()
                {
                    if (rangeData)
                        getHostAllocator(*allocRef).deallocate(reinterpret_cast<typename CPUAllocator::pointer>(rangeData),sizeof(size_type)*numAllocs*2u);
                }

                inline DeferredFreeFunctor& operator=(DeferredFreeFunctor&& other)
                {
                    allocRef    = other.allocRef;
                    rangeData   = other.rangeData;
                    numAllocs   = other.numAllocs;
                    other.allocRef  = nullptr;
                    other.rangeData = nullptr;
                    other.numAllocs = 0u;
                }

                inline bool operator()(size_type& unallocatedSize)
                {
                    operator()();
                    for (size_type i=0u; i<numAllocs; i++)
                    {
                        auto freedSize = rangeData[numAllocs+i];
                        if (unallocatedSize>freedSize)
                            unallocatedSize -= freedSize;
                        else
                        {
                            unallocatedSize = 0u;
                            return true;
                        }
                    }
                    return unallocatedSize==0u;
                }

                inline void operator()()
                {
                    #ifdef _DEBUG
                    assert(allocRef && rangeData);
                    #endif // _DEBUG
                    allocRef->multi_free_addr(numAllocs,rangeData,rangeData+numAllocs);
                }

            private:
                core::HeterogenousMemoryAddressAllocatorAdaptor<BasicAddressAllocator,StreamingGPUBufferAllocator,CPUAllocator>*    allocRef;
                size_type*                                                                                                          rangeData;
                size_type                                                                                                           numAllocs;
        };
        GPUEventDeferredHandlerST<DeferredFreeFunctor> deferredFrees;
};


template< typename _size_type=uint32_t, class CPUAllocator=core::allocator<uint8_t> >
using StreamingTransientDataBufferMT = StreamingTransientDataBufferST<_size_type,CPUAllocator>;


}
}

#endif // __IRR_STREAMING_TRANSIENT_DATA_BUFFER_H__


