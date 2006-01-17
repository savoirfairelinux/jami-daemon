#ifndef __SFLPHONE_H__
#define __SFLPHONE_H__

void sflphone_get_events(void);
void sflphone_ask_version		(void);
void sflphone_list_audiodevs	(void);
void sflphone_set_fullname		(char *);
void sflphone_set_sipuser		(char *);
void sflphone_set_siphost		(char *);
void sflphone_set_password		(char *);
void sflphone_set_proxy			(char *);
void sflphone_set_stun			(char *);
void sflphone_call			(char *);

void sflphone_handle_100		(char *);
void sflphone_handle_101		(char *);
void sflphone_handle_200		(char *);

typedef enum { SFLPHONE_QUIT=0, SFLPHONE_QUIT_SOFTLY } SFLPHONE_QUIT_STATE;
void sflphone_quit(SFLPHONE_QUIT_STATE);

#endif // __SFLPHONE_H__
