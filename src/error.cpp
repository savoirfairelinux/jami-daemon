
#include "error.h"
#include "global.h"
#include "manager.h"
  
#include <string>
using namespace std;

Error::Error (Manager *mngr){
	_mngr = mngr;
	issetError = 0;
} 

int
Error::errorName (Error_enum num_name, char* err) {
#if 1
	string str;
	switch (num_name){
		// Handle opening device errors
		case DEVICE_NOT_OPEN:
			_debug("ERROR: Device Not Open\n");	
			_mngr->displayError("Device not open ");
			issetError = 2;
			break;
		case DEVICE_ALREADY_OPEN:
			_debug ("ERROR: Device Already Open !\n");
			_mngr->displayError("Device already open ");
			issetError = 2;
			break;
		case OPEN_FAILED_DEVICE:
			_debug ("ERROR: Open Failed\n");
			_mngr->displayError("Open device failed ");
			issetError = 2; 
			break;
			
		// Handle ALSA errors
		case PARAMETER_STRUCT_ERROR_ALSA:
			str = str.append("Error with hardware parameter structure: ") + err;
			_mngr->displayError((char*)str.data());
			issetError = 1;
			break;
		case ACCESS_TYPE_ERROR_ALSA:
			str = str.append("Cannot set access type: ") + err;
			_mngr->displayError((char*)str.data());
			issetError = 1;
			break;
		case SAMPLE_FORMAT_ERROR_ALSA:
			str = str.append("Cannot set sample format: ") + err;
			_mngr->displayError((char*)str.data());
			issetError = 1;
			break;
		case SAMPLE_RATE_ERROR_ALSA:
			str = str.append("Cannot set sample rate: ") + err;
			_mngr->displayError((char*)str.data());
			issetError = 1;
			break;
		case CHANNEL_ERROR_ALSA:
			str = str.append("Cannot set channel: ") + err;
			_mngr->displayError((char*)str.data());
			issetError = 1;
			break;
		case PARAM_SETUP_ALSA:
			str = str.append("Cannot set parameters: ") + err;
			_mngr->displayError((char*)str.data());
			issetError = 1;
			break;
		case DROP_ERROR_ALSA:
			str = str.append("Error: drop(): ") + err;
			_mngr->displayError((char*)str.data());
			issetError = 1;
			break;
		case PREPARE_ERROR_ALSA:
			str = str.append("Error: prepare(): ") + err;
			_mngr->displayError((char*)str.data());
			issetError = 1;
			break;

		// Handle OSS errors
		case FRAGMENT_ERROR_OSS:
			str = str.append("Error: SNDCTL_DSP_SETFRAGMENT: ") + err;
			_mngr->displayError((char*)str.data());
			issetError = 1;
			break;
		case SAMPLE_FORMAT_ERROR_OSS:
			str = str.append("Error: SNDCTL_DSP_SETFMT: ") + err;
			_mngr->displayError((char*)str.data());
			issetError = 1;
			break;	
		case CHANNEL_ERROR_OSS:
			str = str.append("Error: SNDCTL_DSP_CHANNELS: ") + err;
			_mngr->displayError((char*)str.data());
			issetError = 1;
			break;	
		case SAMPLE_RATE_ERROR_OSS:
			str = str.append("Error: SNDCTL_DSP_SPEED: ") + err;
			_mngr->displayError((char*)str.data());
			issetError = 1;
			break;
		case GETISPACE_ERROR_OSS:
			str = str.append("Error: SNDCTL_DSP_GETISPACE: ") + err;
			_mngr->displayError((char*)str.data());
			issetError = 1;
			break;
		case GETOSPACE_ERROR_OSS:
			str = str.append("Error: SNDCTL_DSP_GETOSPACE: ") + err;
			_mngr->displayError((char*)str.data());
			issetError = 1;
			break;

		// Handle setup errors
		case HOST_PART_FIELD_EMPTY:
			_mngr->displayError("Fill host part field");
			issetError = 2;
			break;	
		case USER_PART_FIELD_EMPTY:
			_mngr->displayError("Fill user part field");
			issetError = 2;
			break;
		case PASSWD_FIELD_EMPTY:
			_mngr->displayError("Fill password field");
			issetError = 2;
			break; 

		// Handle sip uri 
		case FROM_ERROR:
			_mngr->displayError("Error for 'From' header");
			issetError = 1;
			break;
		case TO_ERROR:
			_mngr->displayError("Error for 'To' header");
			issetError = 1;
			break;

		default:
			issetError = 0;
			break;
	}  
	return issetError;   
#endif
	return 1;
} 
  
