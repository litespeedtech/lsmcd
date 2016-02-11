/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */


#include <util/hashstringmap.h>

struct HashStringMapSyntaxChecker
{
    const char *pKey;
    void checkHashStringMapSyntax()
    {
        HashStringMapSyntaxChecker checker;
        HashStringMap< HashStringMapSyntaxChecker * > intlookup;
        intlookup.begin();
        intlookup.end();
        intlookup.insert("ABC", &checker);
        intlookup.update("CDE", &checker);
        intlookup.find("ABC");
        intlookup.remove("ABC");
        intlookup.release_objects();
    }
};
