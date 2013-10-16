#ifndef WEBUI_H
#define WEBUI_H 1

struct http_server;
struct cfgmgr;

struct webui * webui_create(struct http_server *hs, struct cfgmgr *cfgmgr);
void webui_destroy(struct webui *webui);


#endif /* WEBUI_H */
