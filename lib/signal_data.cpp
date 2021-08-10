/** Signal data.
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

#include <cstring>
#include <cinttypes>

#include <villas/signal_type.hpp>
#include <villas/signal_data.hpp>

using namespace villas::node;

void SignalData::set(enum SignalType type, double val)
{
	switch (type) {
		case SignalType::BOOLEAN:
			this->b = val;
			break;

		case SignalType::FLOAT:
			this->f = val;
			break;

		case SignalType::INTEGER:
			this->i = val;
			break;

		case SignalType::COMPLEX:
			this->z = val;
			break;

		case SignalType::INVALID:
			*this = nan();
			break;
	}
}

void SignalData::cast(enum SignalType from, enum SignalType to)
{
	if (from == to) /* Nothing to do */
		return;

	switch (to) {
		case SignalType::BOOLEAN:
			switch(from) {
				case SignalType::BOOLEAN:
					break;

				case SignalType::INTEGER:
					this->b = this->i;
					break;

				case SignalType::FLOAT:
					this->b = this->f;
					break;

				case SignalType::COMPLEX:
					this->b = std::real(this->z);
					break;

				default: { }
			}
			break;

		case SignalType::INTEGER:
			switch(from) {
				case SignalType::BOOLEAN:
					this->i = this->b;
					break;

				case SignalType::INTEGER:
					break;

				case SignalType::FLOAT:
					this->i = this->f;
					break;

				case SignalType::COMPLEX:
					this->i = std::real(this->z);
					break;

				default: { }
			}
			break;

		case SignalType::FLOAT:
			switch(from) {
				case SignalType::BOOLEAN:
					this->f = this->b;
					break;

				case SignalType::INTEGER:
					this->f = this->i;
					break;

				case SignalType::FLOAT:
					break;

				case SignalType::COMPLEX:
					this->f = std::real(this->z);
					break;

				default: { }
			}
			break;

		case SignalType::COMPLEX:
			switch(from) {
				case SignalType::BOOLEAN:
					this->z = this->b;
					break;

				case SignalType::INTEGER:
					this->z = this->i;
					break;

				case SignalType::FLOAT:
					this->z = this->f;
					break;

				case SignalType::COMPLEX:
					break;

				default: { }
			}
			break;

		default: { }
	}
}

int SignalData::parseString(enum SignalType type, const char *ptr, char **end)
{
	switch (type) {
		case SignalType::FLOAT:
			this->f = strtod(ptr, end);
			break;

		case SignalType::INTEGER:
			this->i = strtol(ptr, end, 10);
			break;

		case SignalType::BOOLEAN:
			this->b = strtol(ptr, end, 10);
			break;

		case SignalType::COMPLEX: {
			float real, imag = 0;

			real = strtod(ptr, end);
			if (*end == ptr)
				return -1;

			ptr = *end;

			if (*ptr == 'i' || *ptr == 'j') {
				imag = real;
				real = 0;

				(*end)++;
			}
			else if (*ptr == '-' || *ptr == '+') {
				imag = strtod(ptr, end);
				if (*end == ptr)
					return -1;

				if (**end != 'i' && **end != 'j')
					return -1;

				(*end)++;
			}

			this->z = std::complex<float>(real, imag);
			break;
		}

		case SignalType::INVALID:
			return -1;
	}

	return 0;
}

int SignalData::parseJson(enum SignalType type, json_t *json)
{
	int ret;

	switch (type) {
		case SignalType::FLOAT:
			this->f = json_number_value(json);
			break;

		case SignalType::INTEGER:
			this->i = json_integer_value(json);
			break;

		case SignalType::BOOLEAN:
			this->b = json_boolean_value(json);
			break;

		case SignalType::COMPLEX: {
			double real, imag;

			json_error_t err;
			ret = json_unpack_ex(json, &err, 0, "{ s: F, s: F }",
				"real", &real,
				"imag", &imag
			);
			if (ret)
				return -2;

			this->z = std::complex<float>(real, imag);
			break;
		}

		case SignalType::INVALID:
			return -1;
	}

	return 0;
}

json_t * SignalData::toJson(enum SignalType type) const
{
	switch (type) {
		case SignalType::INTEGER:
			return json_integer(this->i);

		case SignalType::FLOAT:
			return json_real(this->f);

		case SignalType::BOOLEAN:
			return json_boolean(this->b);

		case SignalType::COMPLEX:
			return json_pack("{ s: f, s: f }",
				"real", std::real(this->z),
				"imag", std::imag(this->z)
			);

		case SignalType::INVALID:
			return nullptr;
	}

	return nullptr;
}

int SignalData::printString(enum SignalType type, char *buf, size_t len, int precision) const
{
	switch (type) {
		case SignalType::FLOAT:
			return snprintf(buf, len, "%.*f", precision, this->f);

		case SignalType::INTEGER:
			return snprintf(buf, len, "%" PRIi64, this->i);

		case SignalType::BOOLEAN:
			return snprintf(buf, len, "%u", this->b);

		case SignalType::COMPLEX:
			return snprintf(buf, len, "%.*f%+.*fi", precision, std::real(this->z), precision, std::imag(this->z));

		default:
			return snprintf(buf, len, "<?>");
	}
}

std::string SignalData::toString(enum SignalType type, int precission) const
{
	char buf[128];

	printString(type, buf, sizeof(buf), precission);

	return buf;
}
