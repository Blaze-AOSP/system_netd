/*	$NetBSD: res_mkquery.c,v 1.6 2006/01/24 17:40:32 christos Exp $	*/

/*
 * Copyright (c) 1985, 1993
 *    The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 * 	This product includes software developed by the University of
 * 	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Portions Copyright (c) 1993 by Digital Equipment Corporation.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies, and that
 * the name of Digital Equipment Corporation not be used in advertising or
 * publicity pertaining to distribution of the document or software without
 * specific, written prior permission.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND DIGITAL EQUIPMENT CORP. DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS.   IN NO EVENT SHALL DIGITAL EQUIPMENT
 * CORPORATION BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

/*
 * Copyright (c) 2004 by Internet Systems Consortium, Inc. ("ISC")
 * Portions Copyright (c) 1996-1999 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <arpa/nameser.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/param.h>
#include <sys/types.h>
#ifdef ANDROID_CHANGES
#include "resolv_private.h"
#else
#include <resolv.h>
#endif
#include <stdio.h>
#include <string.h>

/* Options.  Leave them on. */
#ifndef DEBUG
#define DEBUG
#endif

#ifndef lint
#define UNUSED(a) (void) &a
#else
#define UNUSED(a) a = a
#endif

extern const char* _res_opcodes[];

/*
 * Form all types of queries.
 * Returns the size of the result or -1.
 */
int res_nmkquery(res_state statp, int op, /* opcode of query */
                 const char* dname,       /* domain name */
                 int class, int type,     /* class and type of query */
                 const u_char* data,      /* resource record data */
                 int datalen,             /* length of data */
                 const u_char* newrr_in,  /* new rr for modify or append */
                 u_char* buf,             /* buffer to put query */
                 int buflen)              /* size of buffer */
{
    register HEADER* hp;
    register u_char *cp, *ep;
    register int n;
    u_char *dnptrs[20], **dpp, **lastdnptr;

    UNUSED(newrr_in);

#ifdef DEBUG
    if (statp->options & RES_DEBUG)
        printf(";; res_nmkquery(%s, %s, %s, %s)\n", _res_opcodes[op], dname, p_class(class),
               p_type(type));
#endif
    /*
     * Initialize header fields.
     */
    if ((buf == NULL) || (buflen < HFIXEDSZ)) return (-1);
    memset(buf, 0, HFIXEDSZ);
    hp = (HEADER*) (void*) buf;
    hp->id = htons(res_randomid());
    hp->opcode = op;
    hp->rd = (statp->options & RES_RECURSE) != 0U;
    hp->ad = (statp->options & RES_USE_DNSSEC) != 0U;
    hp->rcode = NOERROR;
    cp = buf + HFIXEDSZ;
    ep = buf + buflen;
    dpp = dnptrs;
    *dpp++ = buf;
    *dpp++ = NULL;
    lastdnptr = dnptrs + sizeof dnptrs / sizeof dnptrs[0];
    /*
     * perform opcode specific processing
     */
    switch (op) {
        case QUERY: /*FALLTHROUGH*/
        case NS_NOTIFY_OP:
            if (ep - cp < QFIXEDSZ) return (-1);
            if ((n = dn_comp(dname, cp, ep - cp - QFIXEDSZ, dnptrs, lastdnptr)) < 0) return (-1);
            cp += n;
            ns_put16(type, cp);
            cp += INT16SZ;
            ns_put16(class, cp);
            cp += INT16SZ;
            hp->qdcount = htons(1);
            if (op == QUERY || data == NULL) break;
            /*
             * Make an additional record for completion domain.
             */
            if ((ep - cp) < RRFIXEDSZ) return (-1);
            n = dn_comp((const char*) data, cp, ep - cp - RRFIXEDSZ, dnptrs, lastdnptr);
            if (n < 0) return (-1);
            cp += n;
            ns_put16(T_NULL, cp);
            cp += INT16SZ;
            ns_put16(class, cp);
            cp += INT16SZ;
            ns_put32(0, cp);
            cp += INT32SZ;
            ns_put16(0, cp);
            cp += INT16SZ;
            hp->arcount = htons(1);
            break;

        case IQUERY:
            /*
             * Initialize answer section
             */
            if (ep - cp < 1 + RRFIXEDSZ + datalen) return (-1);
            *cp++ = '\0'; /* no domain name */
            ns_put16(type, cp);
            cp += INT16SZ;
            ns_put16(class, cp);
            cp += INT16SZ;
            ns_put32(0, cp);
            cp += INT32SZ;
            ns_put16(datalen, cp);
            cp += INT16SZ;
            if (datalen) {
                memcpy(cp, data, (size_t) datalen);
                cp += datalen;
            }
            hp->ancount = htons(1);
            break;

        default:
            return (-1);
    }
    return (cp - buf);
}

#ifdef RES_USE_EDNS0
/* attach OPT pseudo-RR, as documented in RFC2671 (EDNS0). */
#ifndef T_OPT
#define T_OPT 41
#endif

int res_nopt(res_state statp, int n0, /* current offset in buffer */
             u_char* buf,             /* buffer to put query */
             int buflen,              /* size of buffer */
             int anslen)              /* UDP answer buffer size */
{
    register HEADER* hp;
    register u_char *cp, *ep;
    u_int16_t flags = 0;

#ifdef DEBUG
    if ((statp->options & RES_DEBUG) != 0U) printf(";; res_nopt()\n");
#endif

    hp = (HEADER*) (void*) buf;
    cp = buf + n0;
    ep = buf + buflen;

    if ((ep - cp) < 1 + RRFIXEDSZ) return (-1);

    *cp++ = 0; /* "." */

    ns_put16(T_OPT, cp); /* TYPE */
    cp += INT16SZ;
    if (anslen > 0xffff) anslen = 0xffff;
    ns_put16(anslen, cp); /* CLASS = UDP payload size */
    cp += INT16SZ;
    *cp++ = NOERROR; /* extended RCODE */
    *cp++ = 0;       /* EDNS version */
    if (statp->options & RES_USE_DNSSEC) {
#ifdef DEBUG
        if (statp->options & RES_DEBUG) printf(";; res_opt()... ENDS0 DNSSEC\n");
#endif
        flags |= NS_OPT_DNSSEC_OK;
    }
    ns_put16(flags, cp);
    cp += INT16SZ;
#ifdef EDNS0_PADDING
    {
        u_int16_t minlen = (cp - buf) + 3 * INT16SZ;
        u_int16_t extra = minlen % EDNS0_PADDING;
        u_int16_t padlen = (EDNS0_PADDING - extra) % EDNS0_PADDING;
        if (minlen > buflen) {
            return (-1);
        }
        padlen = MIN(padlen, buflen - minlen);
        ns_put16(padlen + 2 * INT16SZ, cp); /* RDLEN */
        cp += INT16SZ;
        ns_put16(NS_OPT_PADDING, cp); /* OPTION-CODE */
        cp += INT16SZ;
        ns_put16(padlen, cp); /* OPTION-LENGTH */
        cp += INT16SZ;
        memset(cp, 0, padlen);
        cp += padlen;
    }
#else
    ns_put16(0, cp); /* RDLEN */
    cp += INT16SZ;
#endif
    hp->arcount = htons(ntohs(hp->arcount) + 1);

    return (cp - buf);
}
#endif
