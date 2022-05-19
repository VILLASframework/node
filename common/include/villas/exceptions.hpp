/** Common exceptions.
 *
 * @file
 * @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache License 2.0
 *********************************************************************************/

#pragma once

#include <string>
#include <sstream>
#include <system_error>
#include <utility>
#include <cerrno>

#include <fmt/core.h>
#include <jansson.h>

#include <villas/config.hpp>

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

class MemoryAllocationError : public RuntimeError {

public:
	MemoryAllocationError() :
		RuntimeError("Failed to allocate memory")
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

	char *msg;

	std::string getMessage() const
	{
		std::stringstream ss;

		ss << std::runtime_error::what() << std::endl;

		if (error.position >= 0) {
			ss << std::endl;
			ss << " " << error.text << " in " << error.source << ":" << error.line << ":" << error.column << std::endl;
		}

		ss << std::endl << " Please consult the user documentation for details: " << std::endl;
		ss << "   " << docUri();

		ss << std::endl;

		return ss.str();
	}

public:
	~ConfigError()
	{
		if (msg)
			free(msg);
	}

	template<typename... Args>
	ConfigError(json_t *s, const std::string &i, const std::string &what = "Failed to parse configuration") :
		std::runtime_error(what),
		id(i),
		setting(s)
	{
		error.position = -1;

		msg = strdup(getMessage().c_str());
	}

	template<typename... Args>
	ConfigError(json_t *s, const std::string &i, const std::string &what, Args&&... args) :
		std::runtime_error(fmt::format(what, std::forward<Args>(args)...)),
		id(i),
		setting(s)
	{
		error.position = -1;

		msg = strdup(getMessage().c_str());
	}

	template<typename... Args>
	ConfigError(json_t *s, const json_error_t &e, const std::string &i, const std::string &what = "Failed to parse configuration") :
		std::runtime_error(what),
		id(i),
		setting(s),
		error(e)
	{
		msg = strdup(getMessage().c_str());
	}

	template<typename... Args>
	ConfigError(json_t *s, const json_error_t &e, const std::string &i, const std::string &what, Args&&... args) :
		std::runtime_error(fmt::format(what, std::forward<Args>(args)...)),
		id(i),
		setting(s),
		error(e)
	{
		msg = strdup(getMessage().c_str());
	}

	std::string docUri() const
	{
		std::string baseUri = "https://villas.fein-aachen.org/doc/jump?";

		return baseUri + id;
	}

	virtual const char * what() const noexcept
	{
		return msg;
	}
};

} /* namespace villas */
