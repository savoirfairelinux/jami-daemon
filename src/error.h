#ifndef __ERROR_H__
#define __ERROR_H__

#include <stdio.h>


typedef enum {
	DEVICE_NOT_OPEN = 0,
	DEVICE_ALREADY_OPEN,
	OPEN_FAILED_DEVICE,
	
	PARAMETER_STRUCT_ERROR_ALSA,
	ACCESS_TYPE_ERROR_ALSA,
	SAMPLE_FORMAT_ERROR_ALSA,
	SAMPLE_RATE_ERROR_ALSA,
	CHANNEL_ERROR_ALSA,
	PARAM_SETUP_ALSA,
	DROP_ERROR_ALSA,
	PREPARE_ERROR_ALSA,

	FRAGMENT_ERROR_OSS,
	SAMPLE_FORMAT_ERROR_OSS,
	CHANNEL_ERROR_OSS,
	SAMPLE_RATE_ERROR_OSS,
	GETISPACE_ERROR_OSS,
	GETOSPACE_ERROR_OSS,

	HOST_PART_FIELD_EMPTY,
	USER_PART_FIELD_EMPTY,
	PASSWD_FIELD_EMPTY,

	FROM_ERROR,
	TO_ERROR

} Error_enum;

class Manager;
class Error {
public: 
	Error (Manager *mngr); 
	~Error (void) {};

	int errorName (Error_enum, char *);
	inline int 	getError (void) 	{ return issetError; }
	inline void setError(int err) 	{ issetError = err; }

private:
	Manager *_mngr;
	int 	issetError;
	
};

#endif // __ERROR_H__
