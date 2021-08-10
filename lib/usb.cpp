/** Helpers for USB node-types
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2021, Institute for Automation of Complex Power Systems, EONERC
 * @license GNU General Public License (version 3)
 *
 * VILLASnode
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

#include <villas/usb.hpp>
#include <villas/log.hpp>

using namespace villas;
using namespace villas::usb;

static
struct libusb_context *context = nullptr;

static
int context_users = 0;

static
Logger logger;

static
enum libusb_log_level spdlog_to_libusb_log_level(Log::Level lvl)
{
	switch (lvl) {
		case Log::Level::trace:
		case Log::Level::debug:
			return LIBUSB_LOG_LEVEL_DEBUG;

		case Log::Level::info:
			return LIBUSB_LOG_LEVEL_INFO;

		case Log::Level::warn:
			return LIBUSB_LOG_LEVEL_WARNING;

		case Log::Level::err:
		case Log::Level::critical:
			return LIBUSB_LOG_LEVEL_ERROR;

		default:
		case Log::Level::off:
			return LIBUSB_LOG_LEVEL_NONE;

	}
}

static
Log::Level libusb_to_spdlog_log_level(enum libusb_log_level lvl)
{
	switch (lvl) {
		case LIBUSB_LOG_LEVEL_ERROR:
			return Log::Level::err;

		case LIBUSB_LOG_LEVEL_WARNING:
			return Log::Level::warn;

		case LIBUSB_LOG_LEVEL_INFO:
			return Log::Level::info;

		case LIBUSB_LOG_LEVEL_DEBUG:
			return Log::Level::debug;

		case LIBUSB_LOG_LEVEL_NONE:
		default:
			return Log::Level::off;
	}
}

static
void log_cb(struct libusb_context *ctx, enum libusb_log_level lvl, const char *str)
{
	auto level = libusb_to_spdlog_log_level(lvl);

	logger->log(level, str);
}

void villas::usb::detach(struct libusb_device_handle *handle, int iface)
{
	int ret;

	ret = libusb_detach_kernel_driver(handle, iface);
	if (ret != LIBUSB_SUCCESS)
		throw Error(ret, "Failed to detach USB device from kernel driver");
}

struct libusb_context * villas::usb::init()
{
	int ret;
	struct libusb_context *ctx;

	logger = logging.get("usb");

	ret = libusb_init(&ctx);
	if (ret)
		throw Error(ret, "Failed to initialize libusb context");

	auto lvl = spdlog_to_libusb_log_level(logger->level());
#if LIBUSBX_API_VERSION < 0x01000106
	libusb_set_debug(ctx, lvl);
#else
	ret = libusb_set_option(ctx, LIBUSB_OPTION_LOG_LEVEL, lvl);
	if (ret)
		throw Error(ret, "Failed to set libusb log level");
#endif

	libusb_set_log_cb(ctx, log_cb, LIBUSB_LOG_CB_GLOBAL);

	return ctx;
}

void villas::usb::deinit_context(struct libusb_context *ctx)
{
	context_users--;

	if (context_users == 0) {
		libusb_exit(ctx);
		context = nullptr;
	}
}

struct libusb_context * villas::usb::get_context()
{
	if (context == nullptr)
		context = init();

	context_users++;

	return context;
}

bool Device::match(const Filter *flt) const
{
	return  (flt->bus < 0        || flt->bus        == getBus()) &&
		(flt->port < 0       || flt->port       == getPort()) &&
		(flt->vendor_id < 0  || flt->vendor_id  == getDescriptor().idVendor) &&
		(flt->product_id < 0 || flt->product_id == getDescriptor().idProduct);
}

std::string Device::getStringDescriptor(uint8_t desc_id) const
{
	int ret;
	unsigned char buf[256];

	ret = libusb_get_string_descriptor_ascii(handle, desc_id, buf, sizeof(buf));
	if (ret != LIBUSB_SUCCESS)
		throw RuntimeError("Failed to get USB string descriptor");

	return (char *) buf;
}
