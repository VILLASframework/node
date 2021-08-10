/** The "stats" API request.
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

extern "C" {
	#include <graphviz/gvc.h>
}

#include <villas/timing.hpp>
#include <villas/api/request.hpp>
#include <villas/api/response.hpp>

namespace villas {
namespace node {
namespace api {

class GraphRequest : public Request {

protected:
	GVC_t *gvc;

	std::string layout;
	std::string format;

public:
	GraphRequest(Session *s) :
		Request(s),
		gvc(gvContext()),
		layout(getQueryArg("layout"))
	{
		if (layout.empty())
			layout = "neato";
	}

	~GraphRequest()
	{
		gvFreeContext(gvc);
	}

	virtual Response * execute()
	{
		if (method != Session::Method::GET)
			throw InvalidMethod(this);

		if (body != nullptr)
			throw BadRequest("Status endpoint does not accept any body data");

		auto *sn = session->getSuperNode();
		auto *graph = sn->getGraph();

		char *data;
		unsigned len;

		std::list<std::string> supportedLayouts = { "circo", "dot", "fdp", "neato", "nop", "nop1", "nop2", "osage", "patchwork", "sfdp", "twopi" };
		std::list<std::string> supportedFormats = { "ps", "eps", "txt", "svg", "svgz", "gif", "png", "jpg", "jpeg", "bmp", "dot", "fig", "json", "pdf" };

		format = matches[1];

		auto lit = std::find(supportedLayouts.begin(), supportedLayouts.end(), layout);
		if (lit == supportedLayouts.end())
			throw BadRequest("Unsupported layout: {}", layout);

		auto fit = std::find(supportedFormats.begin(), supportedFormats.end(), format);
		if (fit == supportedFormats.end())
			throw BadRequest("Unsupported format: {}", format);

		std::string ct = "text/plain";
		if (format == "svg")
			ct = "image/svg+xml";
		else if (format == "eps" || format == "ps")
			ct = "application/postscript";
		else if (format == "txt")
			ct = "text/plain";
		else if (format == "gif")
			ct = "image/gif";
		else if (format == "png")
			ct = "image/png";
		else if (format == "jpg" || format == "jpeg")
			ct = "image/jpeg";
		else if (format == "bmp")
			ct = "image/bmp";
		else if (format == "dot")
			ct = "text/vnd.graphviz";
		else if (format == "json")
			ct = "application/json";
		else if (format == "pdf")
			ct = "application/pdf";

		gvLayout(gvc, graph, layout.c_str());
		gvRenderData(gvc, graph, format.c_str(), &data, &len);

		auto buf = Buffer(data, len);
		auto *resp = new Response(session, HTTP_STATUS_OK, ct, buf);

		if (format == "svgz")
			resp->setHeader("Content-Encoding", "gzip");

#if 0
		gvFreeRenderData(data);
#endif
		gvFreeLayout(gvc, graph);

		return resp;
	}
};

/* Register API request */
static char n[] = "graph";
static char r[] = "/graph\\.([a-z]+)";
static char d[] = "get graph representation of configuration";
static RequestPlugin<GraphRequest, n, r, d> p;

} /* namespace api */
} /* namespace node */
} /* namespace villas */

