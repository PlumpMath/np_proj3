#pragma once

#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <string>
#include <map>
#include "Logger.h"

using namespace std;

class HtmlWrapper {
    public:
        static int Init(string header_msg[5]);
        static int Print(int index, const char* buff, int size, bool bold);
        static int Final();
};
