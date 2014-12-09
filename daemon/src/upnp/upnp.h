#ifndef __UPNP_H__
#define __UPNP_H__

#ifdef __cplusplus
extern "C" {
#endif

void init_upnp (void);
void upnp_add_redir (const char * addr, unsigned int port_external, unsigned int port_internal);
void upnp_rem_redir (unsigned int port);
int upnp_get_entry (unsigned int port);
void upnp_remove_all_entries(const char * remove_desc);

#ifdef __cplusplus
}
#endif

#endif /* __UPNP_H__ */