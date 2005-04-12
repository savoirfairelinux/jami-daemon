
#include "error.h"
  
#include <string>
using namespace std;

Error::Error (Manager *_mngr){
	this->mngr = _mngr;
	issetError = 0;
} 

int
Error::errorName (Error_enum num_name, char* err) {
	string str;
	switch (num_name){
		// Handle opening device errors
		case DEVICE_NOT_OPEN:
			printf ("ERROR: Device Not Open\n");			
			mngr->errorDisplay("Device not open ");
			issetError = 2;
			break;
		case DEVICE_ALREADY_OPEN:
			printf ("ERROR: Device Already Open !\n");
			mngr->errorDisplay("Device already open ");
			issetError = 2;
			break;
		case OPEN_FAILED_DEVICE:
			printf ("ERROR: Open Failed\n");
			mngr->errorDisplay("Open device failed ");
			issetError = 2; 
			break;
			
		// Handle ALSA errors
		case PARAMETER_STRUCT_ERROR_ALSA:
			str = str.append("Error with hardware parameter structure: ") + err;
			mngr->errorDisplay((char*)str.data());
			issetError = 1;
			break;
		case ACCESS_TYPE_ERROR_ALSA:
			str = str.append("Cannot set access type: ") + err;
			mngr->errorDisplay((char*)str.data());
			issetError = 1;
			break;
		case SAMPLE_FORMAT_ERROR_ALSA:
			str = str.append("Cannot set sample format: ") + err;
			mngr->errorDisplay((char*)str.data());
			issetError = 1;
			break;
		case SAMPLE_RATE_ERROR_ALSA:
			str = str.append("Cannot set sample rate: ") + err;
			mngr->errorDisplay((char*)str.data());
			issetError = 1;
			break;
		case CHANNEL_ERROR_ALSA:
			str = str.append("Cannot set channel: ") + err;
			mngr->errorDisplay((char*)str.data());
			issetError = 1;
			break;
		case PARAM_SETUP_ALSA:
			str = str.append("Cannot set parameters: ") + err;
			mngr->errorDisplay((char*)str.data());
			issetError = 1;
			break;
		case DROP_ERROR_ALSA:
			str = str.append("Error: drop(): ") + err;
			mngr->errorDisplay((char*)str.data());
			issetError = 1;
			break;
		case PREPARE_ERROR_ALSA:
			str = str.append("Error: prepare(): ") + err;
			mngr->errorDisplay((char*)str.data());
			issetError = 1;
			break;

		// Handle OSS errors
		case FRAGMENT_ERROR_OSS:
			str = str.append("Error: SNDCTL_DSP_SETFRAGMENT: ") + err;
			mngr->errorDisplay((char*)str.data());
			issetError = 1;
			break;
		case SAMPLE_FORMAT_ERROR_OSS:
			str = str.append("Error: SNDCTL_DSP_SETFMT: ") + err;
			mngr->errorDisplay((char*)str.data());
			issetError = 1;
			break;	
		case CHANNEL_ERROR_OSS:
			str = str.append("Error: SNDCTL_DSP_CHANNELS: ") + err;
			mngr->errorDisplay((char*)str.data());
			issetError = 1;
			break;	
		case SAMPLE_RATE_ERROR_OSS:
			str = str.append("Error: SNDCTL_DSP_SPEED: ") + err;
			mngr->errorDisplay((char*)str.data());
			issetError = 1;
			break;
		case GETISPACE_ERROR_OSS:
			str = str.append("Error: SNDCTL_DSP_GETISPACE: ") + err;
			mngr->errorDisplay((char*)str.data());
			issetError = 1;
			break;
		case GETOSPACE_ERROR_OSS:
			str = str.append("Error: SNDCTL_DSP_GETOSPACE: ") + err;
			mngr->errorDisplay((char*)str.data());
			issetError = 1;
			break;

		// Handle setup errors
		case HOST_PART_FIELD_EMPTY:
			mngr->errorDisplay("Fill host part field");
			issetError = 2;
			break;	
		case USER_PART_FIELD_EMPTY:
			mngr->errorDisplay("Fill user part field");
			issetError = 2;
			break;
		case PASSWD_FIELD_EMPTY:
			mngr->errorDisplay("Fill password field");
			issetError = 2;
			break; 

		// Handle sip uri 
		case FROM_ERROR:
			mngr->errorDisplay("Error for 'From' header");
			issetError = 1;
			break;
		case TO_ERROR:
			mngr->errorDisplay("Error for 'To' header");
			issetError = 1;
			break;

		default:
			issetError = 0;
			break;
	}  
	return issetError;   
} 
  
