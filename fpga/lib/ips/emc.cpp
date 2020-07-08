/** AXI External Memory Controller (EMC)
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017-2020, Steffen Vogel
 * @license GNU General Public License (version 3)
 *
 * VILLASfpga
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *********************************************************************************/

#include <iostream>

#include <villas/plugin.hpp>
#include <villas/fpga/ips/emc.hpp>

using namespace villas::fpga::ip;

// instantiate factory to make available to plugin infrastructure
static EMCFactory factory;

bool
EMC::init()
{
	int ret;
	const uintptr_t base = getBaseAddr(registerMemory);

#ifdef XPAR_XFL_DEVICE_FAMILY_INTEL
#if XFL_TO_ASYNCMODE
		/* Set Flash to Async mode. */
		if (FLASH_MEM_WIDTH == 1) {
			WRITE_FLASH_8(FLASH_BASE_ADDRESS + ASYNC_ADDR, 0x60);
			WRITE_FLASH_8(FLASH_BASE_ADDRESS + ASYNC_ADDR, 0x03);
		}
		else if (FLASH_MEM_WIDTH == 2) {
			WRITE_FLASH_16(FLASH_BASE_ADDRESS + ASYNC_ADDR,
					INTEL_CMD_CONFIG_REG_SETUP);
			WRITE_FLASH_16(FLASH_BASE_ADDRESS + ASYNC_ADDR,
					INTEL_CMD_CONFIG_REG_CONFIRM);
		}
#endif
#endif

	ret = XFlash_Initialize(&xflash, base, 2, 0);
	if (ret != XST_SUCCESS)
		return false;

	return XFlash_IsReady(&xflash);
}

bool
EMC::read(uint32_t offset, uint32_t length, uint8_t *data)
{
	int ret;

	/*
	 * Reset the Flash Device. This clears the ret registers and puts
	 * the device in Read mode.
	 */
	ret = XFlash_Reset(&xflash);
	if (ret != XST_SUCCESS)
		return false;

	/*
	 * Perform the read operation.
	 */
	ret = XFlash_Read(&xflash, offset, length, data);
	if(ret != XST_SUCCESS)
		return false;

	return false;
}

// objcopy -I ihex -O binary somefile.mcs somefile.bin

bool
EMC::flash(uint32_t offset, const std::string &filename)
{
	bool result;
	uint32_t length;
	uint8_t *buffer;

	std::ifstream is(filename, std::ios::binary);
	
	// get length of file:
	is.seekg(0, std::ios::end);
	length = is.tellg();
	
	is.seekg (0, std::ios::beg);
	// allocate memory:
	
	buffer = new uint8_t[length];
	is.read(reinterpret_cast<char *>(buffer), length);
	is.close();

	result = flash(offset, length, buffer);

	delete[] buffer;

	return result;
}

/* Based on xilflash_readwrite_example.c */
bool
EMC::flash(uint32_t offset, uint32_t length, uint8_t *data)
{
	int ret = XST_FAILURE;
	uint32_t start = offset;

	uint8_t *verify_data = new uint8_t[length];

	/* Reset the Flash Device. This clears the ret registers and puts
	 * the device in Read mode. */
	ret = XFlash_Reset(&xflash);
	if (ret != XST_SUCCESS)
		return false;

	/* Perform an unlock operation before the erase operation for the Intel
	 * Flash. The erase operation will result in an error if the block is
	 * locked. */
	if ((xflash.CommandSet == XFL_CMDSET_INTEL_STANDARD) ||
	    (xflash.CommandSet == XFL_CMDSET_INTEL_EXTENDED) ||
	    (xflash.CommandSet == XFL_CMDSET_INTEL_G18)) {
		ret = XFlash_Unlock(&xflash, offset, 0);
		if(ret != XST_SUCCESS)
			return false;
	}

	/* Perform the Erase operation. */
	ret = XFlash_Erase(&xflash, start, length);
	if (ret != XST_SUCCESS)
		return false;

	/* Perform the Write operation. */
	ret = XFlash_Write(&xflash, start, length, data);
	if (ret != XST_SUCCESS)
		return false;

	/* Perform the read operation. */
	ret = XFlash_Read(&xflash, start, length, verify_data);
		if(ret != XST_SUCCESS) {
			return false;
	}

	/* Compare the data read against the data Written. */
	for (unsigned i = 0; i < length; i++) {
		if (verify_data[i] != data[i])
			return false;
	}

	delete[] verify_data;

	return true;
}
