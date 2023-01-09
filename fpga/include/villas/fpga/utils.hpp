/** Helper function for directly using VILLASfpga outside of VILLASnode
 *
 * @file
 * @author Niklas Eiling <niklas.eiling@eonerc.rwth-aachen.de>
 * @copyright 2022, Steffen Vogel, Niklas Eiling
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

#pragma once

#include <string>
#include <villas/fpga/pcie_card.hpp>

namespace villas {
namespace fpga {

std::shared_ptr<fpga::PCIeCard>
setupFpgaCard(const std::string &configFile, const std::string &fpgaName);

void setupColorHandling();

class ConnectString {
public:
	ConnectString(std::string& connectString, int maxPortNum = 7);

	void parseString(std::string& connectString);
	int portStringToInt(std::string &str) const;

	void configCrossBar(std::shared_ptr<villas::fpga::ip::Dma> dma,
		std::vector<std::shared_ptr<fpga::ip::AuroraXilinx>>& aurora_channels) const;

	bool isBidirectional() const { return bidirectional; };
	bool isDmaLoopback() const { return dmaLoopback; };
	bool isSrcStdin() const { return srcIsStdin; };
	bool isDstStdout() const { return dstIsStdout; };
	int getSrcAsInt() const { return srcAsInt; };
	int getDstAsInt() const { return dstAsInt; };
protected:
	villas::Logger log;
	int maxPortNum;
	bool bidirectional;
	bool invert;
	int srcAsInt;
	int dstAsInt;
	bool srcIsStdin;
	bool dstIsStdout;
	bool dmaLoopback;
};

class BufferedSampleFormatter {
public:
	virtual void format(float value) = 0;
	virtual void output(std::ostream& out)
	{
		out << buf.data() << std::flush;
		clearBuf();
	}
	virtual void clearBuf()
	{
		for (size_t i = 0; i < bufSamples && buf[i*bufSampleSize] != '\0'; i++) {
			buf[i*bufSampleSize] = '\0';
		}
		currentBufLoc = 0;
	}
protected:
	std::vector<char> buf;
	const size_t bufSamples;
	const size_t bufSampleSize;
	size_t currentBufLoc;

	BufferedSampleFormatter(const size_t bufSamples, const size_t bufSampleSize) :
		buf(bufSamples*bufSampleSize+1), // Leave room for a final `\0'
		bufSamples(bufSamples),
		bufSampleSize(bufSampleSize),
		currentBufLoc(0) {};
	BufferedSampleFormatter() = delete;
	BufferedSampleFormatter(const BufferedSampleFormatter&) = delete;
	virtual char* nextBufPos()
	{
		return &buf[(currentBufLoc++)*bufSampleSize];
	}
};

class BufferedSampleFormatterShort : public BufferedSampleFormatter {
public:
	BufferedSampleFormatterShort(size_t bufSizeInSamples) :
		BufferedSampleFormatter(bufSizeInSamples, formatStringSize) {};

	virtual void format(float value) override
	{
		if (std::snprintf(nextBufPos(), formatStringSize+1, formatString, value) > (int)formatStringSize) {
			throw RuntimeError("Output buffer too small");
		}
	}

protected:
	static constexpr char formatString[] = "%7f\n";
	static constexpr size_t formatStringSize = 9;
};

class BufferedSampleFormatterLong : public BufferedSampleFormatter {
public:
	BufferedSampleFormatterLong(size_t bufSizeInSamples) :
		BufferedSampleFormatter(bufSizeInSamples, formatStringSize),
		sampleCnt(0) {};

	virtual void format(float value) override
	{
		if (std::snprintf(nextBufPos(), formatStringSize+1, formatString, sampleCnt, value) > (int)formatStringSize) {
			throw RuntimeError("Output buffer too small");
		}
		sampleCnt = (sampleCnt+1)%100000;
	}

protected:
	static constexpr char formatString[] = "%05zd: %7f\n";
	static constexpr size_t formatStringSize = 16;
	size_t sampleCnt;
};

std::unique_ptr<BufferedSampleFormatter> getBufferedSampleFormatter(const std::string &format, size_t bufSizeInSamples)
{
	if (format == "long") {
		return std::make_unique<BufferedSampleFormatterLong>(bufSizeInSamples);
	} else if (format == "short") {
		return std::make_unique<BufferedSampleFormatterShort>(bufSizeInSamples);
	} else {
		throw RuntimeError("Unknown output format '{}'", format);
	}
}

} /* namespace fpga */
} /* namespace villas */
