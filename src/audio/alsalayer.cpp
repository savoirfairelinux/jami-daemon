/*
 *  Copyright (C) 2008 Savoir-Faire Linux inc.
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

void* ringtoneThreadEntry( void *ptr);

static pthread_t ringtone_thread;
bool ringtone_thread_is_running;

pthread_mutex_t mut = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

// Constructor
    AlsaLayer::AlsaLayer( ManagerImpl* manager ) 
    : AudioLayer( manager , ALSA ) 
    , _PlaybackHandle(NULL)
    , _CaptureHandle(NULL)
    , _periodSize()
    , _audioPlugin()
    , _inChannel()
    , _outChannel()
    , _defaultVolume(100)
      , IDSoundCards() 
{
    _debug(" Constructor of AlsaLayer called\n");

    // The flag to stop the ringtone thread loop
    ringtone_thread_is_running = false;
}

// Destructor
AlsaLayer::~AlsaLayer (void) 
{ 
    _debugAlsa("Close ALSA streams\n");
    closeCaptureStream();
    closePlaybackStream();
    deviceClosed = true;

    ringtone_thread_is_running = false;
    pthread_join(ringtone_thread, NULL);
}

    void
AlsaLayer::closeLayer()
{
    _debugAlsa("Close ALSA streams\n");
    closeCaptureStream();
    closePlaybackStream();
    deviceClosed = true;
    
    ringtone_thread_is_running = false;
}

    bool 
AlsaLayer::openDevice (int indexIn, int indexOut, int sampleRate, int frameSize, int stream , std::string plugin) 
{

    // We don't accept that the audio plugin is changed during a conversation
    if( _talk ){
        _debug("can't switch audio plugin when talking\n. Please hang up and try again...\n");
        return false;
    }
        
    if(deviceClosed == false)
    {
        if( stream == SFL_PCM_CAPTURE )
            closeCaptureStream();
        else if( stream == SFL_PCM_PLAYBACK)
            closePlaybackStream();
        else
        {
            closeCaptureStream();
            closePlaybackStream();
        }
    }

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

    
    /*void **hint;
    int r = snd_device_name_hint(-1, "pcm", &hint);

    while( *hint ){
        printf("%s\n", snd_device_name_get_hint(*hint, "DESC"));
        ++hint;
    }*/


    std::string pcmp = buildDeviceTopo( plugin , indexOut , 0);
    std::string pcmc = buildDeviceTopo( PCM_PLUGHW , indexIn , 0);

    return open_device( pcmp , pcmc , stream);
}

    void
AlsaLayer::startStream(void) 
{
    if( _CaptureHandle && _PlaybackHandle )
    {
        _talk = true ;
        _debugAlsa(" Start stream\n");
        int err;
        //ost::MutexLock lock( _mutex );
        snd_pcm_prepare( _CaptureHandle );
        snd_pcm_start( _CaptureHandle ) ;

        snd_pcm_prepare( _PlaybackHandle );
        if( (err = snd_pcm_start( _PlaybackHandle)) < 0 )  _debugAlsa(" Cannot start (%s)\n", snd_strerror(err));
    }
} 

    void
AlsaLayer::stopStream(void) 
{
    if( _CaptureHandle && _PlaybackHandle )
    {
        //ost::MutexLock lock( _mutex );
        _debugAlsa(" Stop Stream\n ");
        _talk = false;
        snd_pcm_drop( _CaptureHandle );
        snd_pcm_prepare( _CaptureHandle );
        snd_pcm_drop( _PlaybackHandle );
        snd_pcm_prepare( _PlaybackHandle );
        _urgentBuffer.flush();
    }
}


void* ringtoneThreadEntry( void *ptr )
{ 
    while( ringtone_thread_is_running )
    {
        ( ( AlsaLayer *) ptr) -> playTones();
        //sleep(0.1);
    }
    /*
    pthread_mutex_lock(&mut);
    while( ((AlsaLayer*)ptr)->_manager->getTelephoneTone() == NULL )
    {
        pthread_cond_wait(&cond, &mut);
    }
    ( AlsaLayer *) ptr -> playTones();
    pthread_mutex_unlock(&mut);*/
    return 0;
}


    void 
AlsaLayer::fillHWBuffer( void)
{

  unsigned char* data;
  int pcmreturn, l1, l2;
  short s1, s2;
  int periodSize = 128 ;
  int frames = periodSize >> 2 ;
  _debug("frames  = %d\n", frames);

  data = (unsigned char*)malloc(periodSize);
  for(l1 = 0; l1 < 100; l1++) {
    for(l2 = 0; l2 < frames; l2++) {
      s1 = 0;
      s2 = 0;
      data[4*l2] = (unsigned char)s1;
      data[4*l2+1] = s1 >> 8;
      data[4*l2+2] = (unsigned char)s2;
      data[4*l2+3] = s2 >> 8;
    }
    while ((pcmreturn = snd_pcm_writei(_PlaybackHandle, data, frames)) < 0) {
      snd_pcm_prepare(_PlaybackHandle);
      //_debugAlsa("< Buffer Underrun >\n");
    }
  }
}

    bool
AlsaLayer::isStreamActive (void) 
{
    ost::MutexLock lock( _mutex );
    return (isPlaybackActive() && isCaptureActive());
}

    int 
AlsaLayer::playSamples(void* buffer, int toCopy, bool isTalking)
{
    //ost::MutexLock lock( _mutex );
    if( isTalking )
        _talk = true;
    if ( _PlaybackHandle ){ 
        write( adjustVolume( buffer , toCopy , SFL_PCM_PLAYBACK ) , toCopy );
    }
    return 0;
}

    int
AlsaLayer::putUrgent(void* buffer, int toCopy)
{
    int nbBytes = 0;

    if ( _PlaybackHandle ){ 
        //fillHWBuffer();
        int a = _urgentBuffer.AvailForPut();
        if( a >= toCopy ){
            nbBytes = _urgentBuffer.Put( buffer , toCopy , _defaultVolume );
        } else {
            nbBytes = _urgentBuffer.Put( buffer , a , _defaultVolume ) ;
        }
    }

    return nbBytes;
}

void AlsaLayer::trigger_thread(void)
{
        _debug("Wake up the ringtone thread\n");
        pthread_cond_broadcast(&cond);
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
        adjustVolume( buffer , toCopy , SFL_PCM_CAPTURE );
    }
    return res ;
}


    bool
AlsaLayer::isStreamStopped (void) 
{
    ost::MutexLock lock( _mutex );
    return !(isStreamActive());
}


void AlsaLayer::reducePulseAppsVolume( void ){}
void AlsaLayer::restorePulseAppsVolume( void ){}

//////////////////////////////////////////////////////////////////////////////////////////////
/////////////////   ALSA PRIVATE FUNCTIONS   ////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////



    void
AlsaLayer::playTones( void )
{
    int frames;
    int maxBytes;

    pthread_mutex_lock(&mut);
    while(!_manager-> getTelephoneTone() && !_manager->getTelephoneFile())
    {
        _debug("Make the ringtone thread wait\n");
        pthread_cond_wait(&cond, &mut);
    }

    //frames = _periodSize  ; 
    frames = 940  ; 
    maxBytes = frames * sizeof(SFLDataFormat)  ;
    SFLDataFormat* out = (SFLDataFormat*)malloc(maxBytes * sizeof(SFLDataFormat));
    if( _talk ) {}
    else {
        AudioLoop *tone = _manager -> getTelephoneTone();
        if( tone != 0 ){
            tone -> getNext( out , frames , _manager->getSpkrVolume() );
            write( out , maxBytes );
        } 
        else if( ( tone=_manager->getTelephoneFile() ) != 0 ){
            tone ->getNext( out , frames , _manager->getSpkrVolume() );
            write( out , maxBytes );
        }
    }
    // free the temporary data buffer 
    free( out ); out = 0;
    pthread_mutex_unlock(&mut);
}


bool
AlsaLayer::isPlaybackActive(void) {
    ost::MutexLock guard( _mutex );
    if( _PlaybackHandle )
        return (snd_pcm_state(_PlaybackHandle) == SND_PCM_STATE_RUNNING ? true : false); 
    else
        return false;
}

bool
AlsaLayer::isCaptureActive(void) {
    ost::MutexLock guard( _mutex );
    if( _CaptureHandle )
        return (snd_pcm_state( _CaptureHandle) == SND_PCM_STATE_RUNNING ? true : false); 
    else
        return false;
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

    if( type == 1 ){
        /*if( (err = snd_async_add_pcm_handler( &_AsyncHandler, pcm_handle , AlsaCallBack, this ) < 0)){
            _debugAlsa(" Unable to install the async callback handler (%s)\n", snd_strerror(err));
            return false;
        }*/
        
        // So the loop could start when the ringtone thread entry function is reached
        ringtone_thread_is_running = true;
        if( pthread_create(&ringtone_thread, NULL, ringtoneThreadEntry, this) != 0 )
        {
            _debug("Unable to start the ringtone posix thread\n");
            return false;
        }
    }

    snd_pcm_sw_params_free( swparams );
    deviceClosed = false;

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
        if( (err = snd_pcm_start( _CaptureHandle ))<0) {
          _debugAlsa( "snd_pcm_start failed\n");
        }
    }

    //TODO something, really!
    _talk = false;

    return true;
}

//TODO	first frame causes broken pipe (underrun) because not enough data are send --> make the handle wait to be ready
    int
AlsaLayer::write(void* buffer, int length)
{
    //if(snd_pcm_state( _PlaybackHandle ) == SND_PCM_STATE_XRUN)
    //handle_xrun_playback();  
    //_debugAlsa("avail = %d - toWrite = %d\n" , snd_pcm_avail_update( _PlaybackHandle ) , length / 2);

    snd_pcm_uframes_t frames = snd_pcm_bytes_to_frames( _PlaybackHandle, length);
    int err = snd_pcm_writei( _PlaybackHandle , buffer , frames );
    switch(err) {
        case -EAGAIN: 
            _debugAlsa("EAGAIN (%s)\n", snd_strerror( err ));
            snd_pcm_resume( _PlaybackHandle );
            break;
        case -EPIPE: 
            _debugAlsa(" UNDERRUN (%s)\n", snd_strerror(err));
            handle_xrun_playback();
            snd_pcm_writei( _PlaybackHandle , buffer , frames );
            break;
        case -ESTRPIPE:
            _debugAlsa(" ESTRPIPE(%s)\n", snd_strerror(err));
            snd_pcm_resume( _PlaybackHandle );
            break;
        case -EBADFD:
            _debugAlsa(" (%s)\n", snd_strerror( err ));
            break;
    }

    if( ( err >=0 ) && ( err < (int)frames ) )
        _debugAlsa("Short write : %d out of %d\n", err , (int)frames);

    return ( err > 0 )? err : 0 ;
}

    int
AlsaLayer::read( void* buffer, int toCopy)
{
    if(deviceClosed || _CaptureHandle == NULL)
        return 0;

    int err;
    if(snd_pcm_state( _CaptureHandle ) == SND_PCM_STATE_XRUN)
    {
        snd_pcm_prepare( _CaptureHandle );
        snd_pcm_start( _CaptureHandle );
    }
    snd_pcm_uframes_t frames = snd_pcm_bytes_to_frames( _CaptureHandle, toCopy );
    if(( err = snd_pcm_readi( _CaptureHandle, buffer, frames)) < 0 ) {
        switch(err){
            case EPERM:
                _debugAlsa(" Capture EPERM (%s)\n", snd_strerror(err));
                snd_pcm_prepare( _CaptureHandle);
                snd_pcm_start( _CaptureHandle );
                break;
            case -EAGAIN:
                _debugAlsa(" Capture EAGAIN (%s)\n", snd_strerror(err));
                break;
            case -EPIPE:
                _debugAlsa(" Capture EPIPE (%s)\n", snd_strerror(err));
                handle_xrun_capture();
                break;
        }
        return 0;
    }

    return toCopy;

}

    int
AlsaLayer::putInCache( char code UNUSED, 
        void *buffer UNUSED, 
        int toCopy UNUSED )
{
    return 1;
}

    void
AlsaLayer::handle_xrun_capture( void )
{
    snd_pcm_status_t* status;
    snd_pcm_status_alloca( &status );

    int res = snd_pcm_status( _CaptureHandle, status );
    if( res <= 0){
        if(snd_pcm_status_get_state(status) == SND_PCM_STATE_XRUN ){
            snd_pcm_drop( _CaptureHandle );
            snd_pcm_prepare( _CaptureHandle );
            snd_pcm_start( _CaptureHandle ); 
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
            snd_pcm_drop( _PlaybackHandle );
            snd_pcm_prepare( _PlaybackHandle );
            //snd_pcm_start( _PlaybackHandle ); 
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

    void
AlsaLayer::closeCaptureStream( void)
{
    if(_CaptureHandle){
        snd_pcm_drop( _CaptureHandle );
        snd_pcm_close( _CaptureHandle );
        _CaptureHandle = 0;
    }
}

    void
AlsaLayer::closePlaybackStream( void)
{
    if(_PlaybackHandle){
        snd_pcm_drop( _PlaybackHandle );
        snd_pcm_close( _PlaybackHandle );
        _PlaybackHandle = 0;
    }
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

    void*
AlsaLayer::adjustVolume( void* buffer , int len, int stream )
{
    int vol;
    if( stream == SFL_PCM_PLAYBACK )
        vol = _manager->getSpkrVolume();
    else
        vol = _manager->getMicVolume();

    SFLDataFormat* src = (SFLDataFormat*) buffer;
    if( vol != 100 )
    {
        int size = len / sizeof(SFLDataFormat);
        int i;
        for( i = 0 ; i < size ; i++ ){
            src[i] = src[i] * vol  / 100 ;
        }
    }
    return src ; 
}

