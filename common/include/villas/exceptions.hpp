/** Common exceptions.
 *
 * @file
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

#pragma once

#include <string>
#include <sstream>
#include <system_error>
#include <utility>
#include <cerrno>

#include <fmt/core.h>
#include <jansson.h>

#include <villas/config.h>

namespace villas {

class SystemError : public std::system_error {

public:
	SystemError(const std::string &what) :
		std::system_error(
			errno,
			std::system_category(),
			what
		)
	{ }

	template<typename... Args>
	SystemError(const std::string &what, Args&&... args) :
		SystemError(fmt::format(what, std::forward<Args>(args)...))
	{ }
};

class RuntimeError : public std::runtime_error {

public:
	template<typename... Args>
	RuntimeError(const std::string &what, Args&&... args) :
		std::runtime_error(fmt::format(what, std::forward<Args>(args)...))
	{ }
};

class JsonError : public std::runtime_error {

protected:
	const json_t *setting;
	json_error_t error;

public:
	template<typename... Args>
	JsonError(const json_t *s, const json_error_t &e, const std::string &what = std::string(), Args&&... args) :
		std::runtime_error(fmt::format(what, std::forward<Args>(args)...)),
		setting(s),
		error(e)
	{ }

	virtual const char * what() const noexcept
	{
		return fmt::format("{}: {} in {}:{}:{}",
			std::runtime_error::what(),
			error.text, error.source, error.line, error.column
		).c_str();
	}
};

class ConfigError : public std::runtime_error {

protected:
	/** A setting-id referencing the setting. */
	std::string id;
	json_t *setting;
	json_error_t error;

public:
	template<typename... Args>
	ConfigError(json_t *s, const std::string &i, const std::string &what = "Failed to parse configuration") :
		std::runtime_error(what),
		id(i),
		setting(s)
	{ }

	template<typename... Args>
	ConfigError(json_t *s, const std::string &i, const std::string &what, Args&&... args) :
		std::runtime_error(fmt::format(what, std::forward<Args>(args)...)),
		id(i),
		setting(s)
	{ }

	template<typename... Args>
	ConfigError(json_t *s, const json_error_t &e, const std::string &i, const std::string &what = "Failed to parse configuration") :
		std::runtime_error(what),
		id(i),
		setting(s),
		error(e)
	{ }

	template<typename... Args>
	ConfigError(json_t *s, const json_error_t &e, const std::string &i, const std::string &what, Args&&... args) :
		std::runtime_error(fmt::format(what, std::forward<Args>(args)...)),
		id(i),
		setting(s),
		error(e)
	{ }

	std::string docUri() const
	{
		std::string baseUri = "https://villas.fein-aachen.org/doc/jump?";

		return baseUri + id;
	}

	virtual const char * what() const noexcept
	{
		std::stringstream ss;

		ss << std::runtime_error::what() << std::endl;
		ss << " Please consult the user documentation for details: " << docUri();

		auto str = new std::string(ss.str());

		return str->c_str();
	}
};

} /* namespace villas */
