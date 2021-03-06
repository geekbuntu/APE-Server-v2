#include "ape_config.h"
#include "ape_server.h"
#include "ape_array.h"
#include <stdlib.h>
#define _GNU_SOURCE
#include <string.h>

static void ape_config_error(cfg_t *cfg, const char *fmt, va_list ap);

int ape_config_server_setup(cfg_t *conf, ape_server *server)
{
    int i = 0;


    for (i = 0; i < cfg_size(conf, "servername"); i++) {
        printf("Host : %s\n", cfg_getnstr(conf, "servername", i));
    }
}

cfg_t *ape_read_config(const char *file, ape_global *ape)
{
    ape_array_t *srvlst;
    cfg_t *cfg;
    cfg_t *server;
    int i;
    char *ipport;

    cfg_opt_t server_opts[] =
    {
        CFG_STR_LIST("servername", "{127.0.0.1}", CFGF_NONE),
        CFG_STR("serverroot", 0, CFGF_NODEFAULT),
        CFG_END()
    };

    cfg_opt_t opts[] =
    {
        CFG_SEC("server", server_opts, CFGF_MULTI | CFGF_TITLE),
        CFG_END()
    };

    cfg = cfg_init(opts, CFGF_NOCASE);

    cfg_set_error_function(cfg, ape_config_error);

    if (cfg_parse(cfg, file) != CFG_SUCCESS) {
        cfg_free(cfg);
        return NULL;
    }

    srvlst = ape_array_new(8);

    for (i = 0; i < cfg_size(cfg, "server"); i++) {
        ape_server *aserver;
        char *sep, ip[16];
        uint16_t port;
        struct in_addr addr4;

        server = cfg_getnsec(cfg, "server", i);

        ipport = strdup(cfg_title(server));

        sep = strchr(ipport, ':');
        *sep = '\0';

        if (sep == ipport) {
            strcpy(ip, "0.0.0.0");
        } else if (inet_pton(AF_INET, ipport, &addr4) == 1) {
            strncpy(ip, ipport, 16);
        } else {
            goto error;
        }

        port = atoi(&sep[1]);
        if (port == 0 || port > 65535) {
            goto error;
        }

        *sep = ':';
        
        //printf("Dir : %s\n", cfg_getstr(server, "serverroot"));

        if ((aserver = (ape_server *)ape_array_lookup(srvlst, ipport,
                                            strlen(ipport))) != NULL ||
                        (aserver = ape_server_init(port, ip, ape)) != NULL) {

            ape_array_add_ptrn(srvlst, ipport, strlen(ipport), aserver);
            
            aserver->chroot = strdup(cfg_getstr(server, "serverroot"));

        }
        free(ipport);
    }

    ape_array_destroy(srvlst);

    return cfg;

error:
    cfg_free(cfg);
    free(ipport);
    return NULL;
}


static void ape_config_error(cfg_t *cfg, const char *fmt, va_list ap)
{
    printf("[Config error] ");
    if (cfg && cfg->filename && cfg->line) {
        printf("%s:%d ", cfg->filename, cfg->line);
    } else if (cfg && cfg->filename) {
        printf("%s ", cfg->filename);
    }
        vprintf(fmt, ap);
        printf("\n");
}

// vim: ts=4 sts=4 sw=4 et

