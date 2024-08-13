#pragma once

#include <string>
#include <iostream>

namespace rtf_filter
{
    // Convert a file stream of a Rich Text Format document to plain text
    bool Convert(std::istream& is, std::ostream& os);
}
