/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is Mozilla.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications.  Portions created by Netscape Communications are
 * Copyright (C) 2001 by Netscape Communications.  All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *   Darin Fisher <darin@netscape.com> (original author)
 */

#include "nsHttpRequestHead.h"

//-----------------------------------------------------------------------------
// nsHttpRequestHead
//-----------------------------------------------------------------------------

void
nsHttpRequestHead::Flatten(nsACString &buf, PRBool pruneProxyHeaders)
{
    // note: the first append is intentional.
 
    buf.Append(mMethod.get());
    buf.Append(' ');
    buf.Append(mRequestURI);
    buf.Append(" HTTP/");

    switch (mVersion) {
    case NS_HTTP_VERSION_1_1:
        buf.Append("1.1");
        break;
    case NS_HTTP_VERSION_0_9:
        buf.Append("0.9");
        break;
    default:
        buf.Append("1.0");
    }

    buf.Append("\r\n");

    mHeaders.Flatten(buf, pruneProxyHeaders);
}

#ifdef CAVISSON
#include <unistd.h>
#include <iostream.h>

void
nsHttpRequestHead::GetShortRequest(nsACString &buf, int cap_fd, int det_fd, PRBool using_ssl)
{
    char write_buffer[64];
    int string_length;
    int count;
    int i;    
    const char* value;
    nsHttpAtom header;
 
    if (cap_fd != -1) {  /* means that we are working on the main url */
        sprintf(write_buffer, "          METHOD=%s,\n", mMethod.get());
        string_length = strlen(write_buffer);
        if (write(cap_fd, write_buffer, string_length) != string_length)
            cout << "CAVISSON: error in writing to capture file" << endl;
        if (write(cap_fd, "          URL=", strlen("          URL=")) != strlen("          URL="))
            cout << "CAVISSON: error in writing to capture file" << endl;
        if (mVersion != NS_HTTP_VERSION_1_1) { /* don't need to find out the host */
            string_length = strlen(mRequestURI.get());
            if (write(cap_fd, mRequestURI.get(), string_length) != string_length)
                cout << "CAVISSON: error in writing to capture file" << endl;
        } else {
            count = mHeaders.Count();
            for (i = 0; i < count; i++) {
                value = mHeaders.PeekHeaderAt(i, header);
                if (strcmp(header.get(), "Host") == 0) {
                    if (using_ssl) {
                        if (write(cap_fd, "https://", strlen("https://")) != strlen("https://"))
                            cout << "CAVISSON: error in writing to capture file" << endl;
                    } else {
                        if (write(cap_fd, "http://", strlen("http://")) != strlen("http://"))
                            cout << "CAVISSON: error in writing to capture file" << endl;
                    }
                    string_length = strlen(value);
                    if (write(cap_fd, value, string_length) != string_length)
                        cout << "CAVISSON: error in writing to capture file" << endl;
                    break;
                }
            }
            string_length = strlen(mRequestURI.get());
            if (write(cap_fd, mRequestURI.get(), string_length) != string_length)
                cout << "CAVISSON: error in writing to capture file" << endl;
        }
        if (write(cap_fd, ",\n", 2) != 2)
            cout << "CAVISSON: error in writing to capture file" << endl;

    } else {
        buf.Append(mMethod.get());
        buf.Append(' ');
        buf.Append(mRequestURI);
        buf.Append(" HTTP/");
        
        switch (mVersion) {
        case NS_HTTP_VERSION_1_1:
            buf.Append("1.1");
            break;
        case NS_HTTP_VERSION_0_9:
            buf.Append("0.9");
            break;
        default:
            buf.Append("1.0");
        }

        buf.Append("\r\n");
    }

    mHeaders.ShortFlatten(buf, cap_fd, det_fd);
}
#endif
