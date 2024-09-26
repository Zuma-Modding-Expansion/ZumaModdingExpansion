#include "Zuma_Prefix.pch"

#include "InterfaceParser.h"

using namespace Sexy;

Sexy::InterfaceParser::InterfaceParser()
{
    i = 0;
    mHasFailed = false;
}

bool InterfaceParser::ParseInterfaceFile(const std::string& theFilename)
{
    mXMLParser = new XMLParser();
    mXMLParser->OpenFile(theFilename);

    XMLElement anElement;
    while (!mHasFailed)//!mXMLParser->HasFailed())
    {
        if (!mXMLParser->NextElement(&anElement))
        {
            //Fail(mXMLParser->GetErrorText());
            mHasFailed = true;
        }

        if (anElement.mType == XMLElement::TYPE_START)
        {
            if (anElement.mValue == "Screen")
            {
                i++;
            }

            break;
        }
    }

    delete mXMLParser;
    mXMLParser = NULL;
    return true;
}