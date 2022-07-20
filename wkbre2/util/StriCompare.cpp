#include "StriCompare.h"


int icompare(const std::string &a, const std::string &b)
{
    #ifdef _WIN32
            return _stricmp(a.c_str(), b.c_str());
    #else
            return strcasecmp(a.c_str(), b.c_str());
    #endif
}
