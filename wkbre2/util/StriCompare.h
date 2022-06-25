// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#pragma once

#include <cstring>
#include <string>

struct StriCompare
{
	bool operator() (const std::string &a, const std::string &b) const {
#ifdef _WIN32
        return _stricmp(a.c_str(), b.c_str()) < 0;
#else
        return strcasecmp(a.c_str(), b.c_str()) < 0;
#endif
	}
};
