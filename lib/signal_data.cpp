/** Signal data.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2020, Institute for Automation of Complex Power Systems, EONERC
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

#include <villas/signal.h>
#include <villas/signal_type.h>
#include <villas/signal_data.h>

void signal_data_set(union signal_data *data, const struct signal *sig, double val)
{
	switch (sig->type) {
		case SignalType::BOOLEAN:
			data->b = val;
			break;

		case SignalType::FLOAT:
			data->f = val;
			break;

		case SignalType::INTEGER:
			data->i = val;
			break;

		case SignalType::COMPLEX:
			data->z = val;
			break;

		case SignalType::INVALID:
			*data = signal_data::nan();
			break;
	}
}

void signal_data_cast(union signal_data *data, const struct signal *from, const struct signal *to)
{
	if (from->type == to->type) /* Nothing to do */
		return;

	switch (to->type) {
		case SignalType::BOOLEAN:
			switch(from->type) {
				case SignalType::BOOLEAN:
					break;

				case SignalType::INTEGER:
					data->b = data->i;
					break;

				case SignalType::FLOAT:
					data->b = data->f;
					break;

				case SignalType::COMPLEX:
					data->b = std::real(data->z);
					break;

				default: { }
			}
			break;

		case SignalType::INTEGER:
			switch(from->type) {
				case SignalType::BOOLEAN:
					data->i = data->b;
					break;

				case SignalType::INTEGER:
					break;

				case SignalType::FLOAT:
					data->i = data->f;
					break;

				case SignalType::COMPLEX:
					data->i = std::real(data->z);
					break;

				default: { }
			}
			break;

		case SignalType::FLOAT:
			switch(from->type) {
				case SignalType::BOOLEAN:
					data->f = data->b;
					break;

				case SignalType::INTEGER:
					data->f = data->i;
					break;

				case SignalType::FLOAT:
					break;

				case SignalType::COMPLEX:
					data->f = std::real(data->z);
					break;

				default: { }
			}
			break;

		case SignalType::COMPLEX:
			switch(from->type) {
				case SignalType::BOOLEAN:
					data->z = data->b;
					break;

				case SignalType::INTEGER:
					data->z = data->i;
					break;

				case SignalType::FLOAT:
					data->z = data->f;
					break;

				case SignalType::COMPLEX:
					break;

				default: { }
			}
			break;

		default: { }
	}
}

int signal_data_parse_str(union signal_data *data, const struct signal *sig, const char *ptr, char **end)
{
	switch (sig->type) {
		case SignalType::FLOAT:
			data->f = strtod(ptr, end);
			break;

		case SignalType::INTEGER:
			data->i = strtol(ptr, end, 10);
			break;

		case SignalType::BOOLEAN:
			data->b = strtol(ptr, end, 10);
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

			data->z = std::complex<float>(real, imag);
			break;
		}

		case SignalType::INVALID:
			return -1;
	}

	return 0;
}

int signal_data_parse_json(union signal_data *data, const struct signal *sig, json_t *cfg)
{
	int ret;

	switch (sig->type) {
		case SignalType::FLOAT:
			data->f = json_real_value(cfg);
			break;

		case SignalType::INTEGER:
			data->i = json_integer_value(cfg);
			break;

		case SignalType::BOOLEAN:
			data->b = json_boolean_value(cfg);
			break;

		case SignalType::COMPLEX: {
			double real, imag;

			json_error_t err;
			ret = json_unpack_ex(cfg, &err, 0, "{ s: F, s: F }",
				"real", &real,
				"imag", &imag
			);
			if (ret)
				return -2;

			data->z = std::complex<float>(real, imag);
			break;
		}

		case SignalType::INVALID:
			return -1;
	}

	return 0;
}

json_t * signal_data_to_json(union signal_data *data, const struct signal *sig)
{
	switch (sig->type) {
		case SignalType::INTEGER:
			return json_integer(data->i);

		case SignalType::FLOAT:
			return json_real(data->f);

		case SignalType::BOOLEAN:
			return json_boolean(data->b);

		case SignalType::COMPLEX:
			return json_pack("{ s: f, s: f }",
				"real", std::real(data->z),
				"imag", std::imag(data->z)
			);

		case SignalType::INVALID:
			return nullptr;
	}

	return nullptr;
}

int signal_data_print_str(const union signal_data *data, const struct signal *sig, char *buf, size_t len)
{
	switch (sig->type) {
		case SignalType::FLOAT:
			return snprintf(buf, len, "%.6f", data->f);

		case SignalType::INTEGER:
			return snprintf(buf, len, "%" PRIi64, data->i);

		case SignalType::BOOLEAN:
			return snprintf(buf, len, "%u", data->b);

		case SignalType::COMPLEX:
			return snprintf(buf, len, "%.6f%+.6fi", std::real(data->z), std::imag(data->z));

		default:
			return snprintf(buf, len, "<?>");
	}
}
