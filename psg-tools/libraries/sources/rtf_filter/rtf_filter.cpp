//#include <fstream>
//#include <cstring>  // needed for strcmp
//#include <algorithm> // needed for std::sort

#include "rtf_filter.h"
#include "ccontrolword.h"

namespace rtf_filter
{
    bool Convert(std::istream& is, std::ostream& os)
    {
        char c = 0;
        long lBraceLevel = 0;
        long lSkipLevel = 2;    // How deeply nested the document sections are (e.g body. header etc..)
        long lSkipSections = 1; // How many sections to skip
        bool bSkipGroup = true;
        unsigned long byteCnt = 0;
        CControlWord cw;
        
        // first fetch first byte to check for RTF format
        is.get(c);
        if (c != '{') 
        {
            // not a RTF document!!
            return false;
        }

        // put the byte back
        is.putback(c);

        // now start processing the file stream 
        while (is.eof() == 0) 
        {
            is.get(c);
            switch (c) 
            {
            case '\\':
            {
                is >> cw;
                // output any user text found while parsing control word
                os << cw;
                if (cw == "*") 
                {
                    // must skip rest of group if not already skipping a parent group
                    if (bSkipGroup != true) 
                    {
                        bSkipGroup = true;
                        lSkipLevel = lBraceLevel;
                    }
                }

                // workaround for non-compliant RTF documents
                std::string strTmp = cw.getString();
                strTmp = strTmp.substr(0, 5);
                if (strTmp == "snext") 
                {
                    // must skip rest of group if not already skipping a parent group
                    if (bSkipGroup != true) 
                    {
                        bSkipGroup = true;
                        lSkipLevel = lBraceLevel;
                    }
                }

                if (cw == "par") 
                {
                    os << " ";
                    byteCnt++;
                }
                else if (cw == "line") 
                {
                    os << " ";
                    byteCnt++;
                }
                else if (cw == "tab") 
                {
                    os << " ";
                    os << " ";
                    byteCnt++;
                }
                else if (cw == "page") 
                {
                    os << " ";
                    os << " ";
                    byteCnt++;
                }
                else if (cw == "cell") 
                {
                    // put some spaces between text in cells of table
                    os << " ";
                    os << " ";
                    byteCnt++;
                }
                else if (cw == "pict" || cw == "info") 
                {
                    // skip info/bitmap informtion
                    if (bSkipGroup == false) 
                    {
                        bSkipGroup = true;
                        lSkipLevel = lBraceLevel;
                    }
                    // else already skipping group
                }
                else if (cw == "rquote") 
                {
                    os << "'";
                }
                cw.putString("");
                break;
            }

            case '{':
                ++lBraceLevel;
                break;

            case '}':
                --lBraceLevel;
                // Cancel SkipGroup since } signifies end of group
                // out only do this if we are coming out of the group
                // and not coming out of some sub group in the skipped group
                if (lSkipLevel > lBraceLevel) 
                {
                    --lSkipSections;
                    if (lSkipSections <= 0) 
                    {
                        bSkipGroup = false;
                    }
                }
                break;

            case '<':
            case '>':
                break;

            default:
                if (bSkipGroup == false) 
                {
                    os << c;
                    byteCnt++;
                }
                break;
            }
        }
        return true;
    }

}