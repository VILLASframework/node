#include <vector>
#include <string>

#include "utils.hpp"

namespace villas {
namespace utils {

std::vector<std::string>
tokenize(std::string s, std::string delimiter)
{
	std::vector<std::string> tokens;

	size_t lastPos = 0;
	size_t curentPos;

	while((curentPos = s.find(delimiter, lastPos)) != std::string::npos) {
		const size_t tokenLength = curentPos - lastPos;
		tokens.push_back(s.substr(lastPos, tokenLength));

		// advance in string
		lastPos = curentPos + delimiter.length();
	}

	// check if there's a last token behind the last delimiter
	if(lastPos != s.length()) {
		const size_t lastTokenLength = s.length() - lastPos;
		tokens.push_back(s.substr(lastPos, lastTokenLength));
	}

	return tokens;
}

} // namespace utils
} // namespace villas
