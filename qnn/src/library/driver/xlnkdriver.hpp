/*
    Copyright (c) 2018, Xilinx, Inc.
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:

    1.  Redistributions of source code must retain the above copyright notice,
        this list of conditions and the following disclaimer.

    2.  Redistributions in binary form must reproduce the above copyright
        notice, this list of conditions and the following disclaimer in the
        documentation and/or other materials provided with the distribution.

    3.  Neither the name of the copyright holder nor the names of its
        contributors may be used to endorse or promote products derived from
        this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
    THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
    PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
    CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
    EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
    PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
    OR BUSINESS INTERRUPTION). HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
    WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
    OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
    ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef XLNKDRIVER_H
#define XLNKDRIVER_H

#include <cstring>
#include <map>

extern "C" {
#include <libxlnk_cma.h>
}


#include "donutdriver.hpp"

/**
 * Interface with the libsds for writing/reading hardware register and mapping
 * contiguous memory into userspace
 */
class XlnkDriver : public DonutDriver {
	public:
		/**
		 * Default constructor, maps the register with the given size into
		 * userspace
		 * @param regBase register address
		 * @param regSize register size
		 */
		XlnkDriver(uint32_t regBase, unsigned int regSize) : m_regSize(regSize) {
			m_reg = reinterpret_cast<volatile AccelReg*>(cma_mmap(regBase, regSize));
			if (!m_reg) {
				throw std::runtime_error("Failed to allocate registers");
			}
			m_numSysRegs = 0;
		};

		/**
		 * frees up all cma buffers and unmaps the register
		 */
		virtual ~XlnkDriver() {
			for (PhysMap::iterator iter = m_physmap.begin(); iter != m_physmap.end(); ++iter) {
				cma_free(iter->second);
			}
			cma_munmap(const_cast<AccelReg*>(m_reg), m_regSize);
		}

		/**
		 * copies an local host buffer to an allocated hardware buffer
		 * @param hostBuffer  local host buffer
		 * @param accelBuffer allocated hardware buffer
		 * @param numBytes    bytes to copy
		 */
		virtual void copyBufferHostToAccel(void* hostBuffer, void* accelBuffer, unsigned int numBytes) {
			PhysMap::iterator iter = m_physmap.find(accelBuffer);
			if (iter == m_physmap.end()) {
				throw std::runtime_error("Invalid buffer specified");
			}
			void* virt = iter->second;
			std::memcpy(virt, hostBuffer, numBytes);
		}

		/**
		 * copies an allocated hardware buffer to a local host buffer
		 * @param hostBuffer  local host buffer
		 * @param accelBuffer allocated hardware buffer
		 * @param numBytes    bytes to copy
		 */
		virtual void copyBufferAccelToHost(void* accelBuffer, void* hostBuffer, unsigned int numBytes) {
			PhysMap::iterator iter = m_physmap.find(accelBuffer);
			if (iter == m_physmap.end()) {
				throw std::runtime_error("Invalid buffer specified");
			}
			void* virt = iter->second;
			std::memcpy(hostBuffer, virt, numBytes);
		}

		/**
		 * allocates a hardware buffer
		 * @param numBytes  bytes to allocate
		 * @param cacheable cacheable or non-cacheable
		 */
		virtual void* allocAccelBuffer(unsigned int numBytes, uint32_t cacheable) {
			void* virt = cma_alloc(numBytes, cacheable); // Changed to try to increase performances
			if (!virt) return 0;
			void* phys = reinterpret_cast<void*>(cma_get_phy_addr(virt));
			m_physmap.insert(std::make_pair(phys, virt));
			// printf("<%s> %p\n", __func__, phys);
			return phys;
		}

		/**
		 * frees a hardware buffer
		 * @param buffer buffer to free
		 */
		virtual void deallocAccelBuffer(void* buffer) {
			PhysMap::iterator iter = m_physmap.find(buffer);
			if (iter == m_physmap.end()) {
				throw std::runtime_error("Invalid pointer freed");
			}
			cma_free(iter->second);
			m_physmap.erase(iter);
		}


		// virtual void flushCache(void* buffer, int size) {
		// 	// printf("<%s> %p\n", __func__, buffer);
		// 	PhysMap::iterator iter = m_physmap.find(buffer);
		// 	if (iter == m_physmap.end()) {
		// 		throw std::runtime_error("Invalid pointer to flush");
		// 	}
		// 	cma_flush_cache(iter->second, size);
		// }
        //
		// virtual void invalidateCache(void* buffer, int size) {
		// 	// printf("<%s> %p\n", __func__, buffer);
		// 	PhysMap::iterator iter = m_physmap.find(buffer);
		// 	if (iter == m_physmap.end()) {
		// 		throw std::runtime_error("Invalid pointer to invalidate");
		// 	}
		// 	cma_invalidate_cache(iter->second, size);
		// }

		/**
		 * Get the physical address of an allocated hardware buffer
		 * @param virt virtual buffer address
		 */
		virtual void* getPhys(void * virt) {
			void* phys = reinterpret_cast<void*>(cma_get_phy_addr(virt));
			return phys;
		}

		/**
		 * Get the virtual address of an allocated hardware buffer
		 * @param phys physical buffer address
		 */
		virtual void* getVirt(void * phys) {
			PhysMap::iterator iter = m_physmap.find(phys);
			if (iter == m_physmap.end()) {
				throw std::runtime_error("Invalid buffer specified");
			}
			void* virt = iter->second;
			return virt;
		}

	protected:
		/**
		 * Writes the mapped register at offset
		 * @param addr     address offset
		 * @param regValue register value
		 */
		virtual void writeRegAtAddr(unsigned int addr, AccelReg regValue) {
			if (addr & 0x3) {
				throw std::runtime_error("Unaligned register write");
			}
			m_reg[addr >> 2] = regValue;
		}

		/**
		 * Reads the mapped register at offset
		 * @param  addr address offset
		 * @return      register value
		 */
		virtual AccelReg readRegAtAddr(unsigned int addr) {
			if (addr & 0x3) {
				throw std::runtime_error("Unaligned register read");
			}
			return m_reg[addr >> 2];
		}

	private:
		typedef std::map<void*, void*> PhysMap;
		PhysMap m_physmap;
		volatile AccelReg* m_reg;
		uint32_t m_regSize;

};



#endif
