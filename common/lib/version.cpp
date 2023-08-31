/* Version.
 *
 * Author: Steffen Vogel <github@daniel-krebs.net>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdexcept>
#include <string>

#include <villas/log.hpp>
#include <villas/utils.hpp>
#include <villas/version.hpp>

using namespace villas::utils;

Version::Version(const std::string &str)
{
	size_t endpos;

	auto comp = tokenize(str, ".");

	if (comp.size() > 3)
		throw std::invalid_argument("Not a valid version string");

	for (unsigned i = 0; i < 3; i++) {
		if (i < comp.size()) {
			components[i] = std::stoi(comp[i], &endpos, 10);

			if (comp[i].begin() + endpos != comp[i].end())
				throw std::invalid_argument("Not a valid version string");
		}
		else
			components[i] = 0;
	}
}

Version::Version(int maj, int min, int pat) :
	components{maj, min, pat}
{

}

int Version::cmp(const Version &lhs, const Version &rhs)
{
	int d;

	for (int i = 0; i < 3; i++) {
		d = lhs.components[i] - rhs.components[i];
		if (d)
			return d;
	}

	return 0;
}
