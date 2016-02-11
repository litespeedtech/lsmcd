/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */


#include "stringmap.h"

struct StringMapSyntaxChecker
{
    const char *pKey;

    void checkStringMapSyntax()
    {
        StringMapSyntaxChecker checker;
        StringMap< StringMapSyntaxChecker *> intlookup;
        intlookup.begin();
        intlookup.end();
        intlookup.insert("ABC", &checker);
        intlookup.insertUpdate("CDE", &checker);
        intlookup.find("ABC");
        intlookup.remove("ABC");
        intlookup.release_objects();
    }

};

