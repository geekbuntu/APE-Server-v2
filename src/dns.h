#ifndef __DNS_H_
#define __DNS_H_

#include "common.h"

typedef int (*ape_gethostbyname_callback)(const char *ip);

int ape_dns_init(ape_global *ape);
void ape_gethostbyname(const char *host, ape_gethostbyname_callback callback, ape_global *ape);

#endif
