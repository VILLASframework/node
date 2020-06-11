#pragma once

#include <villas/memory.hpp>
#include <villas/fpga/ip_node.hpp>

namespace villas {
namespace fpga {
namespace ip {


class Hls : public virtual IpCore
{
public:
	virtual bool init()
	{
		auto& registers = addressTranslations.at(registerMemory);

		controlRegister = reinterpret_cast<ControlRegister*>(registers.getLocalAddr(registerControlAddr));
		globalIntRegister = reinterpret_cast<GlobalIntRegister*>(registers.getLocalAddr(registerGlobalIntEnableAddr));
		ipIntEnableRegister = reinterpret_cast<IpIntRegister*>(registers.getLocalAddr(registerIntEnableAddr));
		ipIntStatusRegister = reinterpret_cast<IpIntRegister*>(registers.getLocalAddr(registerIntStatusAddr));

		setAutoRestart(false);
		setGlobalInterrupt(false);

		return true;
	}

	bool start()
	{
		controlRegister->ap_start = true;
		running = true;

		return true;
	}

	virtual bool isFinished()
	{ updateRunningStatus(); return !running; }


	bool isRunning()
	{ updateRunningStatus(); return running; }


	void setAutoRestart(bool enabled) const
	{ controlRegister->auto_restart = enabled; }


	void setGlobalInterrupt(bool enabled) const
	{ globalIntRegister->globalInterruptEnable = enabled; }


	void setReadyInterrupt(bool enabled) const
	{ ipIntEnableRegister->ap_ready = enabled; }


	void setDoneInterrupt(bool enabled) const
	{ ipIntEnableRegister->ap_done = enabled; }


	bool isIdleBit() const
	{ return controlRegister->ap_idle; }


	bool isReadyBit() const
	{ return controlRegister->ap_ready; }


	/// Warning: the corresponding bit is cleared on read of the register, so if
	/// not used correctly, this function may never return true. Only use this
	/// function if you really know what you are doing!
	bool isDoneBit() const
	{ return controlRegister->ap_done; }


	bool isAutoRestartBit() const
	{ return controlRegister->auto_restart; }

private:
	void updateRunningStatus()
	{
		if (running and isIdleBit())
			running = false;
	}

protected:
	/* Memory block handling */

	static constexpr const char* registerMemory = "Reg";

	virtual std::list<MemoryBlockName> getMemoryBlocks() const
	{ return { registerMemory }; }


public:
	/* Register definitions */

	static constexpr uintptr_t registerControlAddr			= 0x00;
	static constexpr uintptr_t registerGlobalIntEnableAddr	= 0x04;
	static constexpr uintptr_t registerIntEnableAddr		= 0x08;
	static constexpr uintptr_t registerIntStatusAddr		= 0x0c;

	union ControlRegister {
		uint32_t value;
		struct  { uint32_t
			ap_start				: 1,
			ap_done					: 1,
			ap_idle					: 1,
			ap_ready				: 1,
			_res1					: 3,
			auto_restart			: 1,
			_res2					: 24;
		};
	};

	struct GlobalIntRegister { uint32_t
		globalInterruptEnable	: 1,
		_res					: 31;
	};

	struct IpIntRegister { uint32_t
		ap_done : 1,
		ap_ready : 1,
		_res : 30;
	};
protected:
	ControlRegister*	controlRegister;
	GlobalIntRegister*	globalIntRegister;
	IpIntRegister*		ipIntEnableRegister;
	IpIntRegister*		ipIntStatusRegister;

	bool running;
};

} /* namespace ip */
} /* namespace fpga */
} /* namespace villas */
