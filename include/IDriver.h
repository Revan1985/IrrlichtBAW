// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#ifndef __IRR_I_DRIVER_H_INCLUDED__
#define __IRR_I_DRIVER_H_INCLUDED__

#include "IDriverMemoryAllocation.h"
#include "IGPUBuffer.h"
#include "irr/video/StreamingTransientDataBuffer.h"
#include "ITexture.h"
#include "IMultisampleTexture.h"
#include "ITextureBufferObject.h"
#include "IFrameBuffer.h"
#include "IVideoCapabilityReporter.h"
#include "IQueryObject.h"
#include "IGPUTimestampQuery.h"

namespace irr
{
namespace video
{

	//! Interface to the functionality of the graphics API device which does not require the submission of GPU commands onto a queue.
	/** This interface only deals with OpenGL and Vulkan concepts which do not require a command to be recorded in a command buffer
	and then submitted to a command queue, i.e. functions which only require VkDevice or VkPhysicalDevice.
	Examples of such functionality are the creation of buffers, textures, etc.*/
	class IDriver : public virtual core::IReferenceCounted, public IVideoCapabilityReporter
	{
        protected:
            StreamingTransientDataBufferMT<>* defaultDownloadBuffer;
            StreamingTransientDataBufferMT<>* defaultUploadBuffer;


            IDriver() : IVideoCapabilityReporter(), defaultDownloadBuffer(nullptr), defaultUploadBuffer(nullptr) {}

            virtual ~IDriver()
            {
                if (defaultDownloadBuffer)
                    defaultDownloadBuffer->drop();
                if (defaultUploadBuffer)
                    defaultUploadBuffer->drop();
            }
        public:
            //! needs to be "deleted" since its not refcounted by GPU driver internally
            /** Since not owned by any openGL context and hence not owned by driver.
            You normally need to call glFlush() after placing a fence
            \param whether to perform an implicit flush the first time CPU waiting,
            this only works if the first wait is from the same thread as the one which
            placed the fence. **/
            virtual IDriverFence* placeFence(const bool& implicitFlushWaitSameThread=false) = 0;

            static inline IDriverMemoryBacked::SDriverMemoryRequirements getDeviceLocalGPUMemoryReqs()
            {
                IDriverMemoryBacked::SDriverMemoryRequirements reqs;
                reqs.vulkanReqs.alignment = 0;
                reqs.vulkanReqs.memoryTypeBits = 0xffffffffu;
                reqs.memoryHeapLocation = IDriverMemoryAllocation::ESMT_DEVICE_LOCAL;
                reqs.mappingCapability = IDriverMemoryAllocation::EMCF_CANNOT_MAP;
                reqs.prefersDedicatedAllocation = true;
                reqs.requiresDedicatedAllocation = true;
                return reqs;
            }
            static inline IDriverMemoryBacked::SDriverMemoryRequirements getSpilloverGPUMemoryReqs()
            {
                IDriverMemoryBacked::SDriverMemoryRequirements reqs;
                reqs.vulkanReqs.alignment = 0;
                reqs.vulkanReqs.memoryTypeBits = 0xffffffffu;
                reqs.memoryHeapLocation = IDriverMemoryAllocation::ESMT_NOT_DEVICE_LOCAL;
                reqs.mappingCapability = IDriverMemoryAllocation::EMCF_CANNOT_MAP;
                reqs.prefersDedicatedAllocation = true;
                reqs.requiresDedicatedAllocation = true;
                return reqs;
            }
            static inline IDriverMemoryBacked::SDriverMemoryRequirements getUpStreamingMemoryReqs()
            {
                IDriverMemoryBacked::SDriverMemoryRequirements reqs;
                reqs.vulkanReqs.alignment = 0;
                reqs.vulkanReqs.memoryTypeBits = 0xffffffffu;
                reqs.memoryHeapLocation = IDriverMemoryAllocation::ESMT_DEVICE_LOCAL;
                reqs.mappingCapability = IDriverMemoryAllocation::EMCF_CAN_MAP_FOR_WRITE;
                reqs.prefersDedicatedAllocation = true;
                reqs.requiresDedicatedAllocation = true;
                return reqs;
            }
            static inline IDriverMemoryBacked::SDriverMemoryRequirements getDownStreamingMemoryReqs()
            {
                IDriverMemoryBacked::SDriverMemoryRequirements reqs;
                reqs.vulkanReqs.alignment = 0;
                reqs.vulkanReqs.memoryTypeBits = 0xffffffffu;
                reqs.memoryHeapLocation = IDriverMemoryAllocation::ESMT_NOT_DEVICE_LOCAL;
                reqs.mappingCapability = IDriverMemoryAllocation::EMCF_CAN_MAP_FOR_READ|IDriverMemoryAllocation::EMCF_COHERENT|IDriverMemoryAllocation::EMCF_CACHED;
                reqs.prefersDedicatedAllocation = true;
                reqs.requiresDedicatedAllocation = true;
                return reqs;
            }
            static inline IDriverMemoryBacked::SDriverMemoryRequirements getCPUSideGPUVisibleGPUMemoryReqs()
            {
                IDriverMemoryBacked::SDriverMemoryRequirements reqs;
                reqs.vulkanReqs.alignment = 0;
                reqs.vulkanReqs.memoryTypeBits = 0xffffffffu;
                reqs.memoryHeapLocation = IDriverMemoryAllocation::ESMT_NOT_DEVICE_LOCAL;
                reqs.mappingCapability = IDriverMemoryAllocation::EMCF_CAN_MAP_FOR_READ|IDriverMemoryAllocation::EMCF_CAN_MAP_FOR_WRITE|IDriverMemoryAllocation::EMCF_COHERENT|IDriverMemoryAllocation::EMCF_CACHED;
                reqs.prefersDedicatedAllocation = true;
                reqs.requiresDedicatedAllocation = true;
                return reqs;
            }

            //! Best for Mesh data, UBOs, SSBOs, etc.
            virtual IDriverMemoryAllocation* allocateDeviceLocalMemory(const IDriverMemoryBacked::SDriverMemoryRequirements& additionalReqs) {return nullptr;}

            //! If cannot or don't want to use device local memory, then this memory can be used
            /** If the above fails (only possible on vulkan) or we have perfomance hitches due to video memory oversubscription.*/
            virtual IDriverMemoryAllocation* allocateSpilloverMemory(const IDriverMemoryBacked::SDriverMemoryRequirements& additionalReqs) {return nullptr;}

            //! Best for staging uploads to the GPU, such as resource streaming, and data to update the above memory with
            virtual IDriverMemoryAllocation* allocateUpStreamingMemory(const IDriverMemoryBacked::SDriverMemoryRequirements& additionalReqs) {return nullptr;}

            //! Best for staging downloads from the GPU, such as query results, Z-Buffer, video frames for recording, etc.
            virtual IDriverMemoryAllocation* allocateDownStreamingMemory(const IDriverMemoryBacked::SDriverMemoryRequirements& additionalReqs) {return nullptr;}

            //! Should be just as fast to play around with on the CPU as regular malloc'ed memory, but slowest to access with GPU
            virtual IDriverMemoryAllocation* allocateCPUSideGPUVisibleMemory(const IDriverMemoryBacked::SDriverMemoryRequirements& additionalReqs) {return nullptr;}

            //! Low level function used to implement the above, use with caution
            virtual IGPUBuffer* createGPUBuffer(const IDriverMemoryBacked::SDriverMemoryRequirements& initialMreqs, const bool canModifySubData=false) {return nullptr;}


            //! For memory allocations without the video::IDriverMemoryAllocation::EMCF_COHERENT mapping capability flag you need to call this for the writes to become GPU visible
            virtual void flushMappedMemoryRanges(const uint32_t& memoryRangeCount, const video::IDriverMemoryAllocation::MappedMemoryRange* pMemoryRanges) {}

            //! Utility wrapper for the pointer based func
            inline void flushMappedMemoryRanges(const core::vector<video::IDriverMemoryAllocation::MappedMemoryRange>& ranges)
            {
                this->flushMappedMemoryRanges(ranges.size(),ranges.data());
            }


            //! Creates the buffer, allocates memory dedicated memory and binds it at once.
            inline IGPUBuffer* createDeviceLocalGPUBufferOnDedMem(size_t size)
            {
                auto reqs = getDeviceLocalGPUMemoryReqs();
                reqs.vulkanReqs.size = size;
                return this->createGPUBufferOnDedMem(reqs,false);
            }

            //! Creates the buffer, allocates memory dedicated memory and binds it at once.
            inline IGPUBuffer* createSpilloverGPUBufferOnDedMem(size_t size)
            {
                auto reqs = getSpilloverGPUMemoryReqs();
                reqs.vulkanReqs.size = size;
                return this->createGPUBufferOnDedMem(reqs,false);
            }

            //! Creates the buffer, allocates memory dedicated memory and binds it at once.
            inline IGPUBuffer* createUpStreamingGPUBufferOnDedMem(size_t size)
            {
                auto reqs = getUpStreamingMemoryReqs();
                reqs.vulkanReqs.size = size;
                return this->createGPUBufferOnDedMem(reqs,false);
            }

            //! Creates the buffer, allocates memory dedicated memory and binds it at once.
            inline IGPUBuffer* createDownStreamingGPUBufferOnDedMem(size_t size)
            {
                auto reqs = getDownStreamingMemoryReqs();
                reqs.vulkanReqs.size = size;
                return this->createGPUBufferOnDedMem(reqs,false);
            }

            //! Creates the buffer, allocates memory dedicated memory and binds it at once.
            inline IGPUBuffer* createCPUSideGPUVisibleGPUBufferOnDedMem(size_t size)
            {
                auto reqs = getCPUSideGPUVisibleGPUMemoryReqs();
                reqs.vulkanReqs.size = size;
                return this->createGPUBufferOnDedMem(reqs,false);
            }

            //! Low level function used to implement the above, use with caution
            virtual IGPUBuffer* createGPUBufferOnDedMem(const IDriverMemoryBacked::SDriverMemoryRequirements& initialMreqs, const bool canModifySubData=false) {return nullptr;}

            //!
            virtual StreamingTransientDataBufferMT<>* getDefaultDownStreamingBuffer() {return defaultDownloadBuffer;}

            //!
            virtual StreamingTransientDataBufferMT<>* getDefaultUpStreamingBuffer() {return defaultUploadBuffer;}

            //! WARNING, THIS FUNCTION MAY STALL AND BLOCK
            inline void updateBufferRangeViaStagingBuffer(IGPUBuffer* buffer, size_t offset, size_t size, const void* data)
            {
                for (uint32_t uploadedSize=0; uploadedSize<size;)
                {
                    const void* dataPtr = reinterpret_cast<const uint8_t*>(data)+uploadedSize;
                    uint32_t localOffset = video::StreamingTransientDataBufferMT<>::invalid_address;
                    uint32_t alignment = defaultUploadBuffer->max_alignment();
                    uint32_t subSize = std::min(core::alignDown(defaultUploadBuffer->max_size(),alignment),size-uploadedSize);

                    defaultUploadBuffer->multi_place(std::chrono::microseconds(500u),1u,(const void* const*)&dataPtr,&localOffset,&subSize,&alignment);
                    // keep trying again
                    if (localOffset==video::StreamingTransientDataBufferMT<>::invalid_address)
                        continue;

                    // some platforms expose non-coherent host-visible GPU memory, so writes need to be flushed explicitly
                    if (defaultUploadBuffer->needsManualFlushOrInvalidate())
                        this->flushMappedMemoryRanges({{defaultUploadBuffer->getBuffer()->getBoundMemory(),localOffset,subSize}});
                    // after we make sure writes are in GPU memory (visible to GPU) and not still in a cache, we can copy using the GPU to device-only memory
                    this->copyBuffer(defaultUploadBuffer->getBuffer(),buffer,localOffset,offset+uploadedSize,subSize);
                    // this doesn't actually free the memory, the memory is queued up to be freed only after the GPU fence/event is signalled
                    auto fence = this->placeFence();
                    defaultUploadBuffer->multi_free(1u,&localOffset,&subSize,fence);
                    fence->drop();
                    uploadedSize += subSize;
                }
            }

            inline IGPUBuffer* createFilledDeviceLocalGPUBufferOnDedMem(size_t size, const void* data)
            {
                IGPUBuffer*  retval = createDeviceLocalGPUBufferOnDedMem(size);

                updateBufferRangeViaStagingBuffer(retval,0u,size,data);

                return retval;
            }

            //! TODO: make with VkBufferCopy and take a list of multiple copies to carry out (maybe rename to copyBufferRanges)
            virtual void copyBuffer(IGPUBuffer* readBuffer, IGPUBuffer* writeBuffer, const size_t& readOffset, const size_t& writeOffset, const size_t& length) {}


            //! Creates a VAO or InputAssembly for OpenGL and Vulkan respectively
            virtual scene::IGPUMeshDataFormatDesc* createGPUMeshDataFormatDesc(core::LeakDebugger* dbgr=NULL) {return nullptr;}


            //! Creates a framebuffer object with no attachments
            virtual IFrameBuffer* addFrameBuffer() {return nullptr;}


            //these will have to be created by a query pool anyway
            virtual IQueryObject* createPrimitivesGeneratedQuery() {return nullptr;}
            virtual IQueryObject* createXFormFeedbackPrimitiveQuery() {return nullptr;} //depr
            virtual IQueryObject* createElapsedTimeQuery() {return nullptr;}
            virtual IGPUTimestampQuery* createTimestampQuery() {return nullptr;}


            //! Convenience function for releasing all images in a mip chain.
            /**
            \param List of .
            \return .
            Bla bla. */
            static inline void dropWholeMipChain(const core::vector<CImageData*>& mipImages)
            {
                for (core::vector<CImageData*>::const_iterator it=mipImages.begin(); it!=mipImages.end(); it++)
                    (*it)->drop();
            }
            //!
            template< class Iter >
            static inline void dropWholeMipChain(Iter it, Iter limit)
            {
                for (; it!=limit; it++)
                    (*it)->drop();
            }
	};

} // end namespace video
} // end namespace irr


#endif

