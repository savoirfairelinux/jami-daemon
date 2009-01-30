/*
 *  Copyright (C) 2008 2009 Savoir-Faire Linux inc.
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "alsalayer.h"

int framesPerBufferAlsa = 2048;

// Constructor
    AlsaLayer::AlsaLayer( ManagerImpl* manager ) 
    : AudioLayer( manager , ALSA ) 
    , _PlaybackHandle(NULL)
    , _CaptureHandle(NULL)
    , _periodSize()
    , _audioPlugin()
    , _inChannel()
    , _outChannel()
    , IDSoundCards() 
    , _is_prepared_playback (false)
    , _is_running_playback (false)
    , _is_open_playback (false)
    , _is_prepared_capture (false)
    , _is_running_capture (false)
    , _is_open_capture (false)
    , _trigger_request (false)
 
{
    _debug(" Constructor of AlsaLayer called\n");
    /* Instanciate the audio thread */
    _audioThread = new AudioThread (this);
}

// Destructor
AlsaLayer::~AlsaLayer (void) 
{ 
    _debug("Destructor of AlsaLayer called\n");
    closeLayer();
}

    void
AlsaLayer::closeLayer()
{
    _debugAlsa("Close ALSA streams\n");
    
    if (_audioThread)
    {
        _debug("Try to stop audio thread\n");
        _audioThread->stop();
        delete _audioThread; _audioThread=NULL;
    }

    closeCaptureStream();
    closePlaybackStream();
}

    bool 
AlsaLayer::openDevice (int indexIn, int indexOut, int sampleRate, int frameSize, int stream , std::string plugin) 
{
    /* Close the devices before open it */  
    if (stream == SFL_PCM_BOTH && is_capture_open() == true && is_playback_open() == true)
    {
        closeCaptureStream();
        closePlaybackStream();
    }
    else if ((stream == SFL_PCM_CAPTURE || stream == SFL_PCM_BOTH) && is_capture_open() == true)
        closeCaptureStream ();
    else if ((stream == SFL_PCM_PLAYBACK || stream == SFL_PCM_BOTH) && is_playback_open () == true)
        closePlaybackStream ();


    _indexIn = indexIn;
    _indexOut = indexOut;
    _sampleRate = sampleRate;
    _frameSize = frameSize;	
    _audioPlugin = plugin;
    _inChannel = 1;
    _outChannel = 1;

    _debugAlsa(" Setting AlsaLayer: device     in=%2d, out=%2d\n", _indexIn, _indexOut);
    _debugAlsa("                   : alsa plugin=%s\n", _audioPlugin.c_str());
    _debugAlsa("                   : nb channel in=%2d, out=%2d\n", _inChannel, _outChannel);
    _debugAlsa("                   : sample rate=%5d, format=%s\n", _sampleRate, SFLDataFormatString);

    ost::MutexLock lock( _mutex );
    
    std::string pcmp = buildDeviceTopo( plugin , indexOut , 0);
    std::string pcmc = buildDeviceTopo( PCM_PLUGHW , indexIn , 0);

    return open_device( pcmp , pcmc , stream);
}

    void
AlsaLayer::startStream(void) 
{
    _debug ("Start ALSA streams\n");
    prepareCaptureStream ();
    startCaptureStream ();
    startPlaybackStream ();
} 

    void
AlsaLayer::stopStream(void) 
{
    _debug ("Stop ALSA streams\n");
    stopCaptureStream ();
    //stopPlaybackStream ();

    /* Flush the ring buffers */
    flushUrgent();
    flushMain();
}

    int
AlsaLayer::canGetMic()
{
    int avail;
    if ( _CaptureHandle ) {
        avail = snd_pcm_avail_update( _CaptureHandle );
        //printf("%d\n", avail ); 
        if(avail > 0)
            return avail;
        else 
            return 0;  
    }
    else
        return 0;
}

    int 
AlsaLayer::getMic(void *buffer, int toCopy)
{
    int res = 0 ; 
    if( _CaptureHandle ) 
    {
        res = read( buffer, toCopy );
    }
    return res ;
}

void AlsaLayer::reducePulseAppsVolume( void ){}
void AlsaLayer::restorePulseAppsVolume( void ){}

//////////////////////////////////////////////////////////////////////////////////////////////
/////////////////   ALSA PRIVATE FUNCTIONS   ////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////
    
void AlsaLayer::stopCaptureStream (void)
{
    if(_CaptureHandle){
        snd_pcm_drop (_CaptureHandle);
        stop_capture ();
    }
}

void AlsaLayer::closeCaptureStream (void)
{
    if( is_capture_prepared() == true && is_capture_running() == true ) 
        stopCaptureStream ();
    if (is_capture_open())
        snd_pcm_close (_CaptureHandle);

    close_capture ();
}

void AlsaLayer::startCaptureStream (void)
{
    if(_CaptureHandle){
        snd_pcm_start (_CaptureHandle);
        start_capture();
    }
}

void AlsaLayer::prepareCaptureStream (void)
{
    int err;

    err = snd_pcm_prepare (_CaptureHandle);
    if( err<0 )
        _debug("Error preparing the device\n");

    prepare_capture ();
}

void AlsaLayer::stopPlaybackStream (void)
{
    if( _PlaybackHandle){
        snd_pcm_drop (_PlaybackHandle);
        stop_playback ();
    }
}


void AlsaLayer::closePlaybackStream (void)
{
    if( is_playback_prepared() == true && is_playback_running() == true ) 
        stopPlaybackStream ();
    if (is_playback_open()) 
        snd_pcm_close (_PlaybackHandle);

    close_playback ();
}

void AlsaLayer::startPlaybackStream (void)
{
    if( _PlaybackHandle){
        snd_pcm_start (_PlaybackHandle);
        start_playback();
    }
}

void AlsaLayer::preparePlaybackStream (void)
{
    int err;

    err = snd_pcm_prepare (_PlaybackHandle);
    if( err<0 )
        _debug("Error preparing the device\n");

    prepare_playback ();
}

bool AlsaLayer::alsa_set_params( snd_pcm_t *pcm_handle, int type, int rate ){

    snd_pcm_hw_params_t *hwparams = NULL;
    snd_pcm_sw_params_t *swparams = NULL;
    unsigned int exact_ivalue;
    unsigned long exact_lvalue;
    int dir;
    int err;
    int format;
    int periods = 4;
    int periodsize = 1024;

    /* Allocate the snd_pcm_hw_params_t struct */
    snd_pcm_hw_params_malloc( &hwparams );

    _periodSize = 940;
    /* Full configuration space */
    if( (err = snd_pcm_hw_params_any(pcm_handle, hwparams)) < 0) { 
        _debugAlsa(" Cannot initialize hardware parameter structure (%s)\n", snd_strerror(err));
        return false;
    }

    if( (err = snd_pcm_hw_params_set_access( pcm_handle, hwparams, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
        _debugAlsa(" Cannot set access type (%s)\n", snd_strerror(err));
        return false;
    }

    /* Set sample format */
    format = SND_PCM_FORMAT_S16_LE;
    if( (err = snd_pcm_hw_params_set_format( pcm_handle, hwparams, (snd_pcm_format_t)format)) < 0) {
        _debugAlsa(" Cannot set sample format (%s)\n", snd_strerror(err));
        return false;
    }
    
    /* Set sample rate. If we can't set to the desired exact value, we set to the nearest acceptable */
    dir=0;
    rate = getSampleRate();
    exact_ivalue = rate;
    if( (err = snd_pcm_hw_params_set_rate_near( pcm_handle, hwparams, &exact_ivalue, &dir) < 0)) {
        _debugAlsa(" Cannot set sample rate (%s)\n", snd_strerror(err));
        return false;
    }
    if( dir!= 0 ){
        _debugAlsa(" (%i) The choosen rate %d Hz is not supported by your hardware.\nUsing %d Hz instead.\n ",type ,rate, exact_ivalue);
    }

    /* Set the number of channels */
    if( (err = snd_pcm_hw_params_set_channels( pcm_handle, hwparams, 1)) < 0){
        _debugAlsa(" Cannot set channel count (%s)\n", snd_strerror(err));
        return false;
    }

    /* Set the buffer size in frames */
    exact_lvalue = periodsize;
    dir=0;
    if( (err = snd_pcm_hw_params_set_period_size_near( pcm_handle, hwparams, &exact_lvalue , &dir)) < 0) {
        _debugAlsa(" Cannot set period time (%s)\n", snd_strerror(err));
        return false;
    }
    if(dir!=0) {
        _debugAlsa("(%i) The choosen period size %d bytes is not supported by your hardware.\nUsing %d instead.\n ", type, (int)periodsize, (int)exact_lvalue);
    }
    periodsize = exact_lvalue;
    _periodSize = exact_lvalue;
    /* Set the number of fragments */
    exact_ivalue = periods;
    dir=0;
    if( (err = snd_pcm_hw_params_set_periods_near( pcm_handle, hwparams, &exact_ivalue, &dir)) < 0) {
        _debugAlsa(" Cannot set periods number (%s)\n", snd_strerror(err));
        return false;
    }
    if(dir!=0) {
        _debugAlsa(" The choosen period number %i bytes is not supported by your hardware.\nUsing %i instead.\n ", periods, exact_ivalue);
    }
    periods=exact_ivalue;

    /* Set the hw parameters */
    if( (err = snd_pcm_hw_params( pcm_handle, hwparams )) < 0) {
        _debugAlsa(" Cannot set hw parameters (%s)\n", snd_strerror(err));
        return false;
    }

    snd_pcm_hw_params_free( hwparams );

    /* Set the sw parameters */
    snd_pcm_sw_params_malloc( &swparams );
    snd_pcm_sw_params_current( pcm_handle, swparams );

    /* Set the start threshold */
    if( (err = snd_pcm_sw_params_set_start_threshold( pcm_handle, swparams, 2700 /*periodsize*2*/ )) < 0 ) {
        _debugAlsa(" Cannot set start threshold (%s)\n", snd_strerror(err)); 
        return false;
    }
    if( (err = snd_pcm_sw_params( pcm_handle, swparams)) < 0 ) {
        _debugAlsa(" Cannot set sw parameters (%s)\n", snd_strerror(err)); 
        return false;
    }


    snd_pcm_sw_params_free( swparams );

    return true;
}


    bool 
AlsaLayer::open_device(std::string pcm_p, std::string pcm_c, int flag)
{

    int err;
    
    if(flag == SFL_PCM_BOTH || flag == SFL_PCM_PLAYBACK)
    {
        if((err = snd_pcm_open(&_PlaybackHandle, pcm_p.c_str(),  SND_PCM_STREAM_PLAYBACK, 0 )) < 0){
            _debugAlsa("Error while opening playback device %s\n",  pcm_p.c_str());
            setErrorMessage( ALSA_PLAYBACK_DEVICE );
            return false;
        }
        if(!alsa_set_params( _PlaybackHandle, 1, getSampleRate() )){
            _debug("playback failed\n");
            snd_pcm_close( _PlaybackHandle );
            return false;
        }

        open_playback ();
    }

    if(flag == SFL_PCM_BOTH || flag == SFL_PCM_CAPTURE)
    {
        if( (err = snd_pcm_open(&_CaptureHandle,  pcm_c.c_str(),  SND_PCM_STREAM_CAPTURE, 0)) < 0){
            _debugAlsa("Error while opening capture device %s\n",  pcm_c.c_str());
            setErrorMessage( ALSA_CAPTURE_DEVICE );
            return false;
        }
        if(!alsa_set_params( _CaptureHandle, 0, 8000 /*getSampleRate()*/ )){
            _debug("capture failed\n");
            snd_pcm_close( _CaptureHandle );
            return false;
        }

        open_capture ();
        
        startCaptureStream ();
    }

    /* Start the secondary audio thread for callbacks */
    _audioThread->start();

    return true;
}

//TODO	first frame causes broken pipe (underrun) because not enough data are send --> make the handle wait to be ready
    int
AlsaLayer::write(void* buffer, int length)
{


    if (_trigger_request == true)
    {
        _trigger_request = false;
        startPlaybackStream ();
    }

    snd_pcm_uframes_t frames = snd_pcm_bytes_to_frames( _PlaybackHandle, length);
    int err;
    if ((err=snd_pcm_writei( _PlaybackHandle , buffer , frames ))<0) 
    {
        switch(err) {
            case -EPIPE: 
            case -ESTRPIPE: 
            case -EIO: 
                //_debugAlsa(" XRUN playback ignored (%s)\n", snd_strerror(err));
                handle_xrun_playback();
                if (snd_pcm_writei( _PlaybackHandle , buffer , frames )<0)
                    _debugAlsa ("XRUN handling failed\n");
                _trigger_request = true;
                break;
            default:
                //_debugAlsa ("Write error unknown - dropping frames **********************************: %s\n", snd_strerror(err));
                stopPlaybackStream ();
                break;
        }
    }

    return ( err > 0 )? err : 0 ;
}

    int
AlsaLayer::read( void* buffer, int toCopy)
{
    ost::MutexLock lock( _mutex );
    
    int samples;

    if(snd_pcm_state( _CaptureHandle ) == SND_PCM_STATE_XRUN)
    {
        prepareCaptureStream ();
        startCaptureStream ();
    }
    
    snd_pcm_uframes_t frames = snd_pcm_bytes_to_frames( _CaptureHandle, toCopy );
    if(( samples = snd_pcm_readi( _CaptureHandle, buffer, frames)) < 0 ) {
        switch (samples)
        {
            case -EPIPE:
            case -ESTRPIPE:
            case -EIO:
                //_debugAlsa(" XRUN capture ignored (%s)\n", snd_strerror(samples));
                handle_xrun_capture();
                samples = snd_pcm_readi( _CaptureHandle, buffer, frames);
                if (samples<0)  samples=0;
                break;
            default:
                //_debugAlsa ("Error when capturing data ***********************************************: %s\n", snd_strerror(samples));
                stopCaptureStream();
                break;
        }
        return 0;
    }

    return toCopy;

}

    void
AlsaLayer::handle_xrun_capture( void )
{
    snd_pcm_status_t* status;
    snd_pcm_status_alloca( &status );

    int res = snd_pcm_status( _CaptureHandle, status );
    if( res <= 0){
        if(snd_pcm_status_get_state(status) == SND_PCM_STATE_XRUN ){
            stop_capture ();
            prepare_capture ();
            start_capture ();
        }
    }
    else
        _debugAlsa(" Get status failed\n");
}

    void
AlsaLayer::handle_xrun_playback( void )
{
    int state; 
    snd_pcm_status_t* status;
    snd_pcm_status_alloca( &status );

    if( (state = snd_pcm_status( _PlaybackHandle, status )) < 0 )   _debugAlsa(" Error: Cannot get playback handle status (%s)\n" , snd_strerror( state ) );
    else 
    { 
        state = snd_pcm_status_get_state( status );
        if( state  == SND_PCM_STATE_XRUN )
        {
            stopPlaybackStream ();
            preparePlaybackStream ();
            _trigger_request = true;
        }
    }
}

    std::string
AlsaLayer::buildDeviceTopo( std::string plugin, int card, int subdevice )
{
    std::string pcm = plugin;
    std::stringstream ss,ss1;
    if( pcm == "default" || pcm == "pulse")
        return pcm;
    ss << card;
    pcm.append(":");
    pcm.append(ss.str());
    if( subdevice != 0 ){
        pcm.append(",");
        ss1 << subdevice;
        pcm.append(ss1.str());
    }
    return pcm;
}

    std::vector<std::string>
AlsaLayer::getSoundCardsInfo( int stream )
{
    std::vector<std::string> cards_id;
    HwIDPair p;

    snd_ctl_t* handle;
    snd_ctl_card_info_t *info;
    snd_pcm_info_t* pcminfo;
    snd_ctl_card_info_alloca( &info );
    snd_pcm_info_alloca( &pcminfo );

    int numCard = -1 ;
    std::string description;

    if(snd_card_next( &numCard ) < 0 || numCard < 0)
        return cards_id;

    while(numCard >= 0){
        std::stringstream ss;
        ss << numCard;
        std::string name= "hw:";
        name.append(ss.str());

        if( snd_ctl_open( &handle, name.c_str(), 0) == 0 ){
            if( snd_ctl_card_info( handle, info) == 0){
                snd_pcm_info_set_device( pcminfo , 0);
                snd_pcm_info_set_stream( pcminfo, ( stream == SFL_PCM_CAPTURE )? SND_PCM_STREAM_CAPTURE : SND_PCM_STREAM_PLAYBACK );
                if( snd_ctl_pcm_info ( handle ,pcminfo ) < 0) _debugAlsa(" Cannot get info\n");
                else{
                    _debugAlsa("card %i : %s [%s]\n", 
                            numCard, 
                            snd_ctl_card_info_get_id(info),
                            snd_ctl_card_info_get_name( info ));
                    description = snd_ctl_card_info_get_name( info );
                    description.append(" - ");
                    description.append(snd_pcm_info_get_name( pcminfo ));
                    cards_id.push_back( description );
                    // The number of the sound card is associated with a string description 
                    p = HwIDPair( numCard , description );
                    IDSoundCards.push_back( p );
                }
            }
            snd_ctl_close( handle );
        }
        if ( snd_card_next( &numCard ) < 0 ) {
            break;
        }
    }
    return cards_id;
}



    bool
AlsaLayer::soundCardIndexExist( int card , int stream )
{
    snd_ctl_t* handle;
    snd_pcm_info_t *pcminfo;
    snd_pcm_info_alloca( &pcminfo );
    std::string name = "hw:";
    std::stringstream ss;
    ss << card ;
    name.append(ss.str());
    if(snd_ctl_open( &handle, name.c_str(), 0) == 0 ){
        snd_pcm_info_set_stream( pcminfo , ( stream == SFL_PCM_PLAYBACK )? SND_PCM_STREAM_PLAYBACK : SND_PCM_STREAM_CAPTURE );
        if( snd_ctl_pcm_info( handle , pcminfo ) < 0) return false;
        else
            return true;
    }
    else
        return false;
}  

    int
AlsaLayer::soundCardGetIndex( std::string description )
{
    unsigned int i;
    for( i = 0 ; i < IDSoundCards.size() ; i++ )
    {
        HwIDPair p = IDSoundCards[i];
        if( p.second == description )
            return  p.first ;
    }
    // else return the default one
    return 0;
}

void AlsaLayer::audioCallback (void)
{

    int toGet, toPut, urgentAvail, normalAvail, micAvailPut, maxBytes; 
    unsigned short spkrVolume, micVolume;
    AudioLoop *tone;

    SFLDataFormat *out;

    spkrVolume = _manager->getSpkrVolume();
    micVolume  = _manager->getMicVolume();

    // AvailForGet tell the number of chars inside the buffer
    // framePerBuffer are the number of data for one channel (left)
    urgentAvail = _urgentRingBuffer.AvailForGet();
    if (urgentAvail > 0) {
        // Urgent data (dtmf, incoming call signal) come first.     
        toGet = (urgentAvail < (int)(framesPerBufferAlsa * sizeof(SFLDataFormat))) ? urgentAvail : framesPerBufferAlsa * sizeof(SFLDataFormat);
        out =  (SFLDataFormat*)malloc(toGet * sizeof(SFLDataFormat) );
        _urgentRingBuffer.Get(out, toGet, spkrVolume);
        /* Play the sound */
        write( out , toGet );
        free(out); out=0;
        // Consume the regular one as well (same amount of bytes)
        _voiceRingBuffer.Discard(toGet);
    } else {
        tone = _manager->getTelephoneTone();
        toGet = 940  ; 
        maxBytes = toGet * sizeof(SFLDataFormat)  ;
        if ( tone != 0) {
            out = (SFLDataFormat*)malloc(maxBytes * sizeof(SFLDataFormat));
            tone->getNext(out, toGet, spkrVolume);
            write (out , maxBytes);
        } else if ( (tone=_manager->getTelephoneFile()) != 0 ) {
            out =  (SFLDataFormat*)malloc(maxBytes * sizeof(SFLDataFormat) );
            tone->getNext(out, toGet, spkrVolume);
            write (out , maxBytes);
        } else {
            // If nothing urgent, play the regular sound samples
            normalAvail = _voiceRingBuffer.AvailForGet();
            toGet = (normalAvail < (int)(framesPerBufferAlsa * sizeof(SFLDataFormat))) ? normalAvail : framesPerBufferAlsa * sizeof(SFLDataFormat);
            out =  (SFLDataFormat*)malloc(framesPerBufferAlsa * sizeof(SFLDataFormat));

            if (toGet) {
                _voiceRingBuffer.Get(out, toGet, spkrVolume);
                write (out, toGet);
            } else {
                bzero(out, framesPerBufferAlsa * sizeof(SFLDataFormat));
            }
        }
        free(out); out=0;
    }
    
    // Additionally handle the mic's audio stream 
    //micAvailPut = _micRingBuffer.AvailForPut();
    //toPut = (micAvailPut <= (int)(framesPerBufferAlsa * sizeof(SFLDataFormat))) ? micAvailPut : framesPerBufferAlsa * sizeof(SFLDataFormat);
    //_debug("AL: Nb sample: %d char, [0]=%f [1]=%f [2]=%f\n", toPut, in[0], in[1], in[2]);
    //_micRingBuffer.Put(in, toPut, micVolume);
}
