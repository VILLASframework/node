#pragma once

#include <string>
#include <vector>
#include <list>

namespace villas {
namespace utils {

std::vector<std::string>
tokenize(std::string s, std::string delimiter);


template<typename T>
void
assertExcept(bool condition, const T& exception)
{
	if(not condition)
		throw exception;
}

} // namespace utils
} // namespace villas

