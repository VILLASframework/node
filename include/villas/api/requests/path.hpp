/** API Request for paths.
 *
 * @file
 * @author Steffen Vogel <post@steffenvogel.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache 2.0
 *********************************************************************************/

#include <villas/api/request.hpp>

namespace villas {
namespace node {

/* Forward declarations */
class Path;

namespace api {

class PathRequest : public Request {

protected:
	class Path *path;

public:
	using Request::Request;

	virtual void
	prepare();
};

} // namespace api
} // namespace node
} // namespace villas
