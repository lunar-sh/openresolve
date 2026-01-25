/**
 * @file openresolve.c
 * @brief Tiny XMR openalias resolver
 *
 * +---------------------------------------+
 * |     .-.       .-.       .-.           |
 * |    /   \     /   \     /   \     +    |
 * |         \   /     \   /     \   /     |
 * |          "_"       "_"       "_"      |
 * |                                       |
 * |  _   _   _ _  _   _   ___   ___ _  _  |
 * | | | | | | | \| | /_\ | _ \ / __| || | |
 * | | |_| |_| | .` |/ _ \|   /_\__ \ __ | |
 * | |____\___/|_|\_/_/ \_\_|_(_)___/_||_| |
 * |                                       |
 * |                                       |
 * | Lunar RF Labs                         |
 * | Email: root@lunar.sh                  |
 * |                                       |
 * | Research Laboratories                 |
 * | OpenAlias (BTC, XMR): lunar.sh        |
 * | Copyright (C) 2022-2024               |
 * +---------------------------------------+
 *
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright notice,
 *       this list of conditions and the following disclaimer in the documentation
 *       and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef _WIN32
#include <windows.h>
#include <windns.h>
#pragma comment(lib, "dnsapi.lib")
#else
#include <resolv.h>
#include <arpa/nameser.h>
#include <netinet/in.h>
#include <errno.h>
#include <strings.h>
#endif

#define MAX_RECORDS      16
#define STR_MAX          256
#define STR_TICKER_MAX    8
#define FQDN_MAX        253
#define LABEL_MAX        63
#define DNS_BUF_SIZE   8192
#define VERSION       "1.0"

typedef struct {
    char currency[STR_TICKER_MAX];
    char address[STR_MAX];
} openalias_record;

#ifdef _WIN32
#define str_icmp  _stricmp
#define str_nicmp _strnicmp
#else
#define str_icmp  strcasecmp
#define str_nicmp strncasecmp
#endif

static void normalize_input(char* s) {
    for (; *s; ++s)
        if (*s == '@')
            *s = '.';
}

static void normalize_ticker(char* s) {
    for (; *s; ++s)
        *s = (char)toupper((unsigned char)*s);
}

static int is_valid_label(const char* s, size_t len) {
    if (len == 0 || len > LABEL_MAX)
        return 0;

    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)s[i];

        if (!isalnum(c) && c != '-')
            return 0;

        if ((i == 0 || i == len - 1) && c == '-')
            return 0;
    }

    return 1;
}

static int is_valid_fqdn(const char* s) {
    size_t len = strlen(s);
    if (len < 3 || len > FQDN_MAX)
        return 0;

    const char* label = s;
    for (const char* p = s; ; ++p) {
        if (*p == '.' || *p == '\0') {
            if (!is_valid_label(label, p - label))
                return 0;
            if (*p == '\0')
                return 1;
            label = p + 1;
        }
    }
}

static int starts_with(const char* s, const char* prefix) {
    return str_nicmp(s, prefix, strlen(prefix)) == 0;
}

static int parse_openalias(const char* txt,
    openalias_record* rec,
    const char* filter) {

    if (!txt || !rec || !starts_with(txt, "oa1:")) return 0;

    const char* p = txt + 4;
    char currency[STR_TICKER_MAX] = { 0 };
    size_t i = 0;

    while (*p && !isspace((unsigned char)*p) && *p != ';' && i < STR_TICKER_MAX - 1)
        currency[i++] = (char)toupper((unsigned char)*p++);
    currency[i] = '\0';

    if (currency[0] == '\0') return 0;
    if (filter && str_icmp(filter, currency) != 0) return 0;

    const char* addr = strstr(txt, "recipient_address=");
    if (!addr) return 0;

    addr += strlen("recipient_address=");
    i = 0;
    while (*addr && *addr != ';' && !isspace((unsigned char)*addr) && i < STR_MAX - 1)
        rec->address[i++] = *addr++;
    rec->address[i] = '\0';

    if (rec->address[0] == '\0') return 0;

    snprintf(rec->currency, STR_TICKER_MAX, "%s", currency);
    rec->currency[STR_TICKER_MAX - 1] = '\0';
    return 1;
}

int main(int argc, char** argv) {

    if (argc < 2) {
        fprintf(stderr, "openalias address lookup v%s\n", VERSION);
        fprintf(stderr, "usage: %s <domain> [-t ticker] (default=xmr)\n", argv[0]);
        return EXIT_FAILURE;
    }

    if (strlen(argv[1]) > FQDN_MAX) {
        fprintf(stderr, "error: domain name too long\n");
        return EXIT_FAILURE;
    }

    char fqdn[FQDN_MAX + 1] = { 0 };
    strncpy(fqdn, argv[1], FQDN_MAX);
    fqdn[FQDN_MAX] = '\0';
    normalize_input(fqdn);

    if (!is_valid_fqdn(fqdn)) {
        fprintf(stderr, "error: invalid fqdn\n");
        return EXIT_FAILURE;
    }

    char filter_buf[STR_TICKER_MAX] = "XMR";
    const char* filter = filter_buf;

    if (argc == 4 && strcmp(argv[2], "-t") == 0) {
        if (strlen(argv[3]) >= STR_TICKER_MAX) {
            fprintf(stderr, "error: ticker too long\n");
            return EXIT_FAILURE;
        }
        strncpy(filter_buf, argv[3], STR_TICKER_MAX - 1);
        filter_buf[STR_TICKER_MAX - 1] = '\0';
        normalize_ticker(filter_buf);
    }

    openalias_record results[MAX_RECORDS] = { 0 };
    size_t count = 0;

#ifdef _WIN32
    DNS_RECORDA* records = NULL;
    DNS_STATUS status = DnsQuery_A(
        fqdn,
        DNS_TYPE_TEXT,
        DNS_QUERY_STANDARD,
        NULL,
        (PDNS_RECORD*) & records,
        NULL
    );

    if (status != ERROR_SUCCESS) {
        fprintf(stderr, "error: dns txt lookup failed (status %lu)\n", status);
        return EXIT_FAILURE;
    }

    for (DNS_RECORDA* r = records; r && count < MAX_RECORDS; r = r->pNext) {
        if (r->wType != DNS_TYPE_TEXT) continue;

        char txt[STR_MAX] = { 0 };
        size_t pos = 0;

        for (DWORD i = 0; i < r->Data.TXT.dwStringCount; i++) {
            const char* seg = r->Data.TXT.pStringArray[i];
            size_t seglen = strlen(seg);
            if (pos + seglen >= STR_MAX) break;
            memcpy(txt + pos, seg, seglen);
            pos += seglen;
        }
        txt[pos] = '\0';

        if (parse_openalias(txt, &results[count], filter))
            count++;
    }

    DnsRecordListFree(records, DnsFreeRecordList);

#else
    unsigned char answer[DNS_BUF_SIZE];
    int len = res_query(fqdn, ns_c_in, ns_t_txt, answer, sizeof(answer));
    if (len < 0) {
        fprintf(stderr, "error: dns txt lookup failed (errno %d)\n", errno);
        return EXIT_FAILURE;
    }

    ns_msg msg;
    if (ns_initparse(answer, len, &msg) < 0) {
        fprintf(stderr, "error: dns parse failed\n");
        return EXIT_FAILURE;
    }

    int rr_count = ns_msg_count(msg, ns_s_an);
    for (int i = 0; i < rr_count && count < MAX_RECORDS; i++) {
        ns_rr rr;
        if (ns_parserr(&msg, ns_s_an, i, &rr) != 0) continue;
        if (ns_rr_type(rr) != ns_t_txt) continue;

        const unsigned char* rdata = ns_rr_rdata(rr);
        int rdlen = ns_rr_rdlen(rr);
        char txt[STR_MAX] = { 0 };
        size_t pos = 0;

        for (int off = 0; off < rdlen;) {
            int seglen = rdata[off++];
            if (seglen <= 0 || off + seglen > rdlen || pos + seglen >= STR_MAX) break;
            memcpy(txt + pos, rdata + off, seglen);
            pos += seglen;
            off += seglen;
        }
        txt[pos] = '\0';

        if (parse_openalias(txt, &results[count], filter))
            count++;
    }
#endif

    if (count == 0) {
        fprintf(stderr, "error: no openalias records found for %s\n", filter);
        return EXIT_FAILURE;
    }

    for (size_t i = 0; i < count; i++)
        puts(results[i].address);

    return EXIT_SUCCESS;
}


