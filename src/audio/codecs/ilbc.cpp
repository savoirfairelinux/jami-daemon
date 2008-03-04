#include "audiocodec.h"

extern "C" {
#include "ilbc/iLBC_encode.h"
#include "ilbc/iLBC_decode.h"
}

class Ilbc: public AudioCodec
{
public:
	Ilbc(int payload)
	: AudioCodec(payload, "iLBC"){
		_clockRate = 8000;
		_channel = 1;
		_bitrate = 13.3;
		_bandwidth = 31.3;
		
		initILBC();

	}

	void initILBC(){
		initDecode(ilbc_dec, 20, 1);
		initEncode(ilbc_enc, 20);
	}

	virtual int codecDecode(short *dst, unsigned char *src, unsigned int size){
		iLBC_decode((float*) dst, src, ilbc_dec, 0);
		return 160;
	}

	virtual int codecEncode(unsigned char *dst, short* src, unsigned int size){
		iLBC_encode(dst, (float*) src, ilbc_enc);
		return 160;
        }	

private:
	iLBC_Dec_Inst_t* ilbc_dec;
	iLBC_Enc_Inst_t* ilbc_enc;
};

// the class factories
extern "C" AudioCodec* create(){
   return new Ilbc(97);
}

extern "C" void destroy(AudioCodec* a){
   delete a;
} 

