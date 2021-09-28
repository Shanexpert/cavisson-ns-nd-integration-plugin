/* This file is for websocket callback */

void opencbfn_type (const char* connectionID, int isbinary, const char * data, int length)
{

}

void errorcbfn_type (const char* connectionID, const char * message, int length)
{

}

void closefn_type (const char* connectionID, int isClosedByClient, int code, const char* reason, int length)
{

}

void sendcbfn_type (const char* connectionID, const char *AccumulatedHeadersStr, int AccumulatedHeadersLen)
{

}

