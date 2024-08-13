#pragma once

#include <string>
#include <iostream>

// Used for parsing RTF documents
// Gets text and extracts displayable text

class CControlWord 
{
public:
    std::string getString();
    void putString(std::string&);
    void putString(const char *);
    bool operator == (const char *);
    friend std::ostream& operator << (std::ostream&, CControlWord&);
    friend std::istream& operator >> (std::istream&, CControlWord&);

private:
    std::string	theWord; // RTF Control Word + Parameter data
    std::string	theText; // Displayeable Text found
}; 
