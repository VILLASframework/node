/** The "status" API request.
 *
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

#include <time.h>
#include <uuid/uuid.h>
#include <sys/utsname.h>
#include <sys/sysinfo.h>

#include <villas/timing.hpp>
#include <villas/api/request.hpp>
#include <villas/api/response.hpp>

typedef char uuid_string_t[37];

namespace villas {
namespace node {
namespace api {

class StatusRequest : public Request {

protected:

#ifdef LWS_WITH_SERVER_STATUS
	json_t * getLwsStatus()
	{
		int ret;

		struct lws_context *ctx = lws_get_context(session->wsi);
		char buf[4096];

		ret = lws_json_dump_context(ctx, buf, sizeof(buf), 0);
		if (ret <= 0)
			throw Error(HTTP_STATUS_INTERNAL_SERVER_ERROR, "Failed to dump LWS context");

		return json_loads(buf, 0, nullptr);
	}
#endif /* LWS_WITH_SERVER_STATUS */

public:
	using Request::Request;

	virtual Response * execute()
	{
		int ret;

		if (method != Session::Method::GET)
			throw InvalidMethod(this);

		if (body != nullptr)
			throw BadRequest("Status endpoint does not accept any body data");

		auto *sn = session->getSuperNode();

		uuid_t uuid;
		uuid_string_t uuid_str;
		struct utsname uts;
		struct sysinfo sinfo;
		char hname[128];

		sn->getUUID(uuid);
		uuid_unparse_lower(uuid, uuid_str);

		auto now = time_now();
		auto started = sn->getStartTime();

		ret = gethostname(hname, sizeof(hname));
		if (ret)
			throw Error(HTTP_STATUS_INTERNAL_SERVER_ERROR, "Failed to get system hostname");

		ret = uname(&uts);
		if (ret)
			throw Error(HTTP_STATUS_INTERNAL_SERVER_ERROR, "Failed to get kernel information");

		ret = sysinfo(&sinfo);
		if (ret)
			throw Error(HTTP_STATUS_INTERNAL_SERVER_ERROR, "Failed to get system information");

		float f_load = 1.f / (1 << SI_LOAD_SHIFT);

		tzset();

		json_error_t err;
		json_t *json_status = json_pack_ex(&err, 0, "{ s: s, s: s, s: s, s: s, s: s, s: s, s: s, s: f, s: f, s: { s: s, s: I, s: b }, s: { s: s, s: s, s: s, s: s, s: s, s: s}, s: { s: i, s: i, s: I, s: I, s: [ f, f, f ], s: { s: I, s, I, s: I, s: I }, s: { s: I, s: I }, s: { s: I, s: I } } }",
			"state", stateToString(sn->getState()).c_str(),
			"version", PROJECT_VERSION_STR,
			"release", PROJECT_RELEASE,
			"build_id", PROJECT_BUILD_ID,
			"build_date", PROJECT_BUILD_DATE,
			"hostname", hname,
			"uuid", uuid_str,
			"time_now", time_to_double(&now),
			"time_started", time_to_double(&started),
			"timezone",
				"name", tzname[daylight],
				"offset", (json_int_t) timezone,
				"dst", daylight,
			"kernel",
				"sysname", uts.sysname,
				"nodename", uts.nodename,
				"release", uts.release,
				"version", uts.version,
				"machine", uts.machine,
				"domainname", uts.domainname,
			"system",
				"cores_configured", get_nprocs_conf(),
				"cores", get_nprocs(),
				"processes", (json_int_t) sinfo.procs,
				"uptime", (json_int_t) sinfo.uptime,
				"load",
					f_load * sinfo.loads[0],
					f_load * sinfo.loads[1],
					f_load * sinfo.loads[2],
				"ram",
					"total", (json_int_t) (sinfo.totalram * sinfo.mem_unit),
					"free", (json_int_t) (sinfo.freeram * sinfo.mem_unit),
					"shared", (json_int_t) (sinfo.sharedram * sinfo.mem_unit),
					"buffer", (json_int_t) (sinfo.bufferram * sinfo.mem_unit),
				"swap",
					"total", (json_int_t) (sinfo.totalswap * sinfo.mem_unit),
					"free", (json_int_t) (sinfo.freeswap * sinfo.mem_unit),
				"highmem",
					"total", (json_int_t) (sinfo.totalhigh * sinfo.mem_unit),
					"free", (json_int_t) (sinfo.freehigh * sinfo.mem_unit)
		);
		if (!json_status)
			throw Error(HTTP_STATUS_INTERNAL_SERVER_ERROR, "Failed to prepare response: {}", err.text);

#ifdef LWS_WITH_SERVER_STATUS
		json_object_set(json_status, "lws", getLwsStatus());
#endif /* LWS_WITH_SERVER_STATUS */

		return new JsonResponse(session, HTTP_STATUS_OK, json_status);
	}
};

/* Register API request */
static char n[] = "status";
static char r[] = "/status";
static char d[] = "get status and statistics of web server";
static RequestPlugin<StatusRequest, n, r, d> p;

} /* namespace api */
} /* namespace node */
} /* namespace villas */

