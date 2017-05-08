#ifndef PROC_MCP_H
#define PROC_MCP_H

struct buffer;
struct mh_router;
struct mh_control;
struct proc_mcp;

struct proc_mcp *mcp_create(struct mh_control *ctl, int channel);
void mcp_destroy(struct proc_mcp *mcp);
void mcp_cb(struct mh_router *router, struct buffer *b, void *user_data);

#endif // PROC_MCP_H
