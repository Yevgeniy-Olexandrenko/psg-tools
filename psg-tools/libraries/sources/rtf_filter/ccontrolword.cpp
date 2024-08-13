#include "ccontrolword.h"

// Auxilliary function
int	CharToHex(unsigned char c) 
{
    // converts hex digit to an int
    int retval = 0;
    switch (c) {
    case '0':
        retval = 0;
        break;
    case '1':
        retval = 1;
        break;
    case '2':
        retval = 2;
        break;
    case '3':
        retval = 3;
        break;
    case '4':
        retval = 4;
        break;
    case '5':
        retval = 5;
        break;
    case '6':
        retval = 6;
        break;
    case '7':
        retval = 7;
        break;
    case '8':
        retval = 8;
        break;
    case '9':
        retval = 9;
        break;
    case 'a':
        retval = 10;
        break;
    case 'b':
        retval = 11;
        break;
    case 'c':
        retval = 12;
        break;
    case 'd':
        retval = 13;
        break;
    case 'e':
        retval = 14;
        break;
    case 'f':
        retval = 15;
        break;
    }
    return retval;
}

std::string	CControlWord::getString() 
{
    return theWord;
}

bool CControlWord::operator == (const char* cstr) 
{
    return (strcmp(theWord.c_str(), cstr) == 0);
}

void CControlWord::putString(std::string& str) 
{
    theWord = str;
}

void CControlWord::putString(const char* cstr) 
{
    theWord = cstr;
}

std::ostream& operator << (std::ostream& os, CControlWord& cw) 
{
    // output displayable text (not RTF keywords)
    os << cw.theText;
    return os;
}

std::istream& operator >> (std::istream& is, CControlWord& cw) 
{
    char c;
    bool inASCIIchar = false;
    bool inFirstChar = true;
    bool finished = false;

    while (finished == false) 
    {
        is.get(c);
        if (is.eof() != 0) 
        {
            // end of file reached
            finished = true;
        }
        else 
        {
            switch (c) 
            {
            case '\'':
                // Converts 'xx to ASCII character
                if (inASCIIchar == true) 
                {
                    // get last 4 bits of character
                    int tmp = CharToHex(c);
                    tmp = tmp & c;
                    cw.theText += (char)tmp;
                    inASCIIchar = false;
                }
                else 
                {
                    // get first four bits of char
                    int tmp = CharToHex(c);
                    tmp = c;
                    inASCIIchar = true;
                }
                break;

            case '\\':
                // Start of another control word
                // put character back in stream
                is.putback(c);
                finished = true;
                break;

            case '{':
                // end of control word found
                // put character back in stream
                is.putback(c);
                finished = true;
                break;

            case '}':
                // end of control word found
                // put character back in stream
                is.putback(c);
                finished = true;
                break;

            default:
                if (c == ' ') 
                {
                    // space - end of control word (also part of the control word)
                    // Ignore character to simplify control word recognition
                    finished = true;
                }
                else if (c == '-' || isdigit(c) != 0) 
                {
                    // start of a numeric parameter (part of the control word)
                    cw.theWord += c;
                }
                else if (isalnum(c) != 0) 
                {
                    // alphanumeric found, part of control word
                    cw.theWord += c;
                }
                else if (c == '*') 
                {
                    // Have found a destination
                    cw.theWord += '*';
                }
                else 
                {
                    // remaining character must be delimiting control word
                    // but is not part of it.
                    // Ignore character
                    finished = true;
                }
                break;
            }
        }
        inFirstChar = false;
    }
    return is;
}
