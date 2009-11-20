/*
 *  Copyright (C) 2009 Savoir-Faire Linux inc.
 *  Author : Alexandre Savard <alexandre.savard@savoirfairelinux.com>
 *
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

#include "mainbuffer.h"

MainBuffer::MainBuffer() : _internalSamplingRate (0)
{
    mixBuffer = new SFLDataFormat[STATIC_BUFSIZE];
}


MainBuffer::~MainBuffer()
{

    delete [] mixBuffer;
    mixBuffer = NULL;
}


void MainBuffer::setInternalSamplingRate (int sr)
{
    ost::MutexLock guard (_mutex);

    if (sr != _internalSamplingRate) {
        // flushAllBuffers();
        _internalSamplingRate = sr;

        flushAllBuffers();

    }

    // flushAllBuffers();
}

CallIDSet* MainBuffer::getCallIDSet (CallID call_id)
{

    CallIDMap::iterator iter = _callIDMap.find (call_id);

    if (iter != _callIDMap.end())
        return iter->second;
    else
        return NULL;

}

bool MainBuffer::createCallIDSet (CallID set_id)
{
    _debug ("---- MainBuffer::createCallIDSet %s\n", set_id.c_str());

    CallIDSet* newCallIDSet = new CallIDSet;

    _callIDMap.insert (pair<CallID, CallIDSet*> (set_id, newCallIDSet));
    // _callIDMap[set_id] = new CallIDSet;

    return true;

}

bool MainBuffer::removeCallIDSet (CallID set_id)
{

    _debug ("---- MainBuffer::removeCallIDSet %s\n", set_id.c_str());


    CallIDSet* callid_set = getCallIDSet (set_id);

    if (callid_set != NULL) {
        if (_callIDMap.erase (set_id) != 0) {
            _debug ("          callid set %s erased!\n", set_id.c_str());
            return true;
        } else {
            _debug ("          error while removing callid set %s!\n", set_id.c_str());
            return false;
        }
    } else {
        _debug ("          callid set %s does not exist!\n", set_id.c_str());
        return false;
    }

}

void MainBuffer::addCallIDtoSet (CallID set_id, CallID call_id)
{
    _debug ("---- MainBuffer::addCallIDtoSet %s in %s\n", set_id.c_str(), call_id.c_str());

    CallIDSet* callid_set = getCallIDSet (set_id);
    callid_set->insert (call_id);

}

void MainBuffer::removeCallIDfromSet (CallID set_id, CallID call_id)
{
    _debug ("---- MainBuffer::removeCallIDfromSet element %s from %s\n", call_id.c_str(), set_id.c_str());

    CallIDSet* callid_set = getCallIDSet (set_id);

    if (callid_set != NULL) {
        if (callid_set->erase (call_id) != 0) {
            _debug ("          callid %s erased from set %s!\n", call_id.c_str(), set_id.c_str());
        } else {
            _debug ("          error while removing callid %s from set %s!\n", call_id.c_str(), set_id.c_str());
        }
    } else {
        _debug ("          callid set %s does not exist!\n", set_id.c_str());
    }
}


RingBuffer* MainBuffer::getRingBuffer (CallID call_id)
{

    RingBufferMap::iterator iter = _ringBufferMap.find (call_id);

    if (iter == _ringBufferMap.end()) {
        // _debug("ringBuffer with ID: \"%s\" doesn't exist! \n", call_id.c_str());
        return NULL;
    } else
        return iter->second;
}


RingBuffer* MainBuffer::createRingBuffer (CallID call_id)
{

    _debug ("---- MainBuffer::createRingBuffer callid %s\n", call_id.c_str());

    RingBuffer* newRingBuffer = new RingBuffer (SIZEBUF, call_id);

    _ringBufferMap.insert (pair<CallID, RingBuffer*> (call_id, newRingBuffer));

    return newRingBuffer;
}


bool MainBuffer::removeRingBuffer (CallID call_id)
{

    _debug ("---- MainBuffer::removeRingBuffer call_id %s\n", call_id.c_str());

    RingBuffer* ring_buffer = getRingBuffer (call_id);

    if (ring_buffer != NULL) {
        if (_ringBufferMap.erase (call_id) != 0) {
            _debug ("          ringbuffer %s removed!\n", call_id.c_str());
            return true;
        } else {
            _debug ("          error while deleting ringbuffer %s!\n", call_id.c_str());
            return false;
        }
    } else {
        _debug ("          error ringbuffer %s does not exist!\n", call_id.c_str());
        return true;
    }
}


void MainBuffer::bindCallID (CallID call_id1, CallID call_id2)
{

    ost::MutexLock guard (_mutex);

    _debug ("---- MainBuffer::bindCallID %s and callid %s\n", call_id1.c_str(), call_id2.c_str());

    RingBuffer* ring_buffer;
    CallIDSet* callid_set;

    if ( (ring_buffer = getRingBuffer (call_id1)) == NULL)
        createRingBuffer (call_id1);

    if ( (callid_set = getCallIDSet (call_id1)) == NULL)
        createCallIDSet (call_id1);

    if ( (ring_buffer = getRingBuffer (call_id2)) == NULL)
        createRingBuffer (call_id2);

    if ( (callid_set = getCallIDSet (call_id2)) == NULL)
        createCallIDSet (call_id2);

    getRingBuffer (call_id1)->createReadPointer (call_id2);

    getRingBuffer (call_id2)->createReadPointer (call_id1);

    addCallIDtoSet (call_id1, call_id2);

    addCallIDtoSet (call_id2, call_id1);

}


void MainBuffer::unBindCallID (CallID call_id1, CallID call_id2)
{

    ost::MutexLock guard (_mutex);

    _debug ("---- MainBuffer::unBindCallID %s and callid %s\n", call_id1.c_str(), call_id2.c_str());

    removeCallIDfromSet (call_id1, call_id2);
    removeCallIDfromSet (call_id2, call_id1);

    RingBuffer* ringbuffer;

    ringbuffer = getRingBuffer (call_id2);

    if (ringbuffer != NULL) {

        ringbuffer->removeReadPointer (call_id1);

        if (ringbuffer->getNbReadPointer() == 0) {
            removeCallIDSet (call_id2);
            removeRingBuffer (call_id2);
        }

    }

    ringbuffer = getRingBuffer (call_id1);

    if (ringbuffer != NULL) {
        ringbuffer->removeReadPointer (call_id2);

        if (ringbuffer->getNbReadPointer() == 0) {
            removeCallIDSet (call_id1);
            removeRingBuffer (call_id1);
        }
    }


}

void MainBuffer::unBindAll (CallID call_id)
{

    ost::MutexLock guard (_mutex);

    CallIDSet* callid_set = getCallIDSet (call_id);

    if (callid_set == NULL)
        return;

    if (callid_set->empty())
        return;

    CallIDSet temp_set = *callid_set;

    CallIDSet::iterator iter_set = temp_set.begin();

    _debug ("MainBuffer::unBindAll\n");

    while (iter_set != temp_set.end()) {
        CallID call_id_in_set = *iter_set;
        unBindCallID (call_id, call_id_in_set);

        iter_set++;
    }

}


int MainBuffer::putData (void *buffer, int toCopy, unsigned short volume, CallID call_id)
{

    ost::MutexLock guard (_mutex);

    RingBuffer* ring_buffer = getRingBuffer (call_id);

    if (ring_buffer == NULL) {
        return 0;
    }

    int a;

    // ost::MutexLock guard (_mutex);
    a = ring_buffer->AvailForPut();

    if (a >= toCopy) {

        return ring_buffer->Put (buffer, toCopy, volume);

    } else {

        return ring_buffer->Put (buffer, a, volume);
    }

}

int MainBuffer::availForPut (CallID call_id)
{

    ost::MutexLock guard (_mutex);

    RingBuffer* ringbuffer = getRingBuffer (call_id);

    if (ringbuffer == NULL)
        return 0;
    else
        return ringbuffer->AvailForPut();

}


int MainBuffer::getData (void *buffer, int toCopy, unsigned short volume, CallID call_id)
{
    ost::MutexLock guard (_mutex);

    CallIDSet* callid_set = getCallIDSet (call_id);

    int nbSmplToCopy = toCopy / sizeof (SFLDataFormat);

    if (callid_set == NULL)
        return 0;

    if (callid_set->empty()) {
        return 0;
    }

    if (callid_set->size() == 1) {

        CallIDSet::iterator iter_id = callid_set->begin();

        if (iter_id != callid_set->end()) {
            return getDataByID (buffer, toCopy, volume, *iter_id, call_id);
        } else
            return 0;
    } else {

        for (int k = 0; k < nbSmplToCopy; k++) {
            ( (SFLDataFormat*) (buffer)) [k] = 0;
        }

        int size = 0;

        CallIDSet::iterator iter_id = callid_set->begin();

        while (iter_id != callid_set->end()) {

            size = getDataByID (mixBuffer, toCopy, volume, (CallID) (*iter_id), call_id);

            if (size > 0) {
                for (int k = 0; k < nbSmplToCopy; k++) {
                    ( (SFLDataFormat*) (buffer)) [k] += mixBuffer[k];
                }
            }

            iter_id++;
        }

        return size;
    }
}


int MainBuffer::getDataByID (void *buffer, int toCopy, unsigned short volume, CallID call_id, CallID reader_id)
{

    // _debug("MainBuffer::getDataByID in buffer %s by %s \n", call_id.c_str(), reader_id.c_str());

    RingBuffer* ring_buffer = getRingBuffer (call_id);

    if (ring_buffer == NULL) {
        // _debug("Output RingBuffer ID: \"%s\" does not exist!\n", call_id.c_str());
        return 0;
    }

    return ring_buffer->Get (buffer, toCopy, volume, reader_id);

    return 0;

}


int MainBuffer::availForGet (CallID call_id)
{

    ost::MutexLock guard (_mutex);

    CallIDSet* callid_set = getCallIDSet (call_id);

    if (callid_set == NULL)
        return 0;

    if (callid_set->empty()) {
        _debug ("CallIDSet with ID: \"%s\" is empty!\n", call_id.c_str());
        return 0;
    }

    if (callid_set->size() == 1) {
        CallIDSet::iterator iter_id = callid_set->begin();
        // _debug("MainBuffer::availForGet availForGetByID(%s,%s)\n", (*iter_id).c_str(), call_id.c_str());

        if ( (call_id != default_id) && (*iter_id == call_id)) {
            _debug ("This problem should not occur since we have %i element\n", (int) callid_set->size());
        }

        // else
        return availForGetByID (*iter_id, call_id);
    } else {
        // _debug("CallIDSet with ID: \"%s\" is a conference!\n", call_id.c_str());
        int avail_bytes = 99999;
        int nb_bytes;
        CallIDSet::iterator iter_id = callid_set->begin();

        for (iter_id = callid_set->begin(); iter_id != callid_set->end(); iter_id++) {
            nb_bytes = availForGetByID (*iter_id, call_id);

            if ( (nb_bytes != 0) && (nb_bytes < avail_bytes))
                avail_bytes = nb_bytes;
        }

        return avail_bytes != 99999 ? avail_bytes : 0;
    }

}


int MainBuffer::availForGetByID (CallID call_id, CallID reader_id)
{

    if ( (call_id != default_id) && (reader_id == call_id)) {
        _debug ("**********************************************************************\n");
        _debug ("Error an RTP session ring buffer is not supposed to have a readpointer on tiself\n");
    }

    RingBuffer* ringbuffer = getRingBuffer (call_id);

    if (ringbuffer == NULL) {
        _debug ("Error: ring buffer does not exist\n");
        return 0;
    } else
        return ringbuffer->AvailForGet (reader_id);

}


int MainBuffer::discard (int toDiscard, CallID call_id)
{
    // _debug("MainBuffer::discard\n");

    ost::MutexLock guard (_mutex);

    CallIDSet* callid_set = getCallIDSet (call_id);

    if (callid_set == NULL)
        return 0;

    if (callid_set->empty()) {
        // _debug("CallIDSet with ID: \"%s\" is empty!\n", call_id.c_str());
        return 0;
    }


    if (callid_set->size() == 1) {
        CallIDSet::iterator iter_id = callid_set->begin();
        // _debug("Discard Data in \"%s\" RingBuffer for \"%s\" ReaderPointer\n",(*iter_id).c_str(),call_id.c_str());
        return discardByID (toDiscard, *iter_id, call_id);
    } else {
        // _debug("CallIDSet with ID: \"%s\" is a conference!\n", call_id.c_str());
        CallIDSet::iterator iter_id;

        for (iter_id = callid_set->begin(); iter_id != callid_set->end(); iter_id++) {
            discardByID (toDiscard, *iter_id, call_id);
        }

        return toDiscard;
    }

}


int MainBuffer::discardByID (int toDiscard, CallID call_id, CallID reader_id)
{

    RingBuffer* ringbuffer = getRingBuffer (call_id);

    if (ringbuffer == NULL)
        return 0;
    else
        return ringbuffer->Discard (toDiscard, reader_id);

}



void MainBuffer::flush (CallID call_id)
{
    ost::MutexLock guard (_mutex);

    // _debug("MainBuffer::flush\n");

    CallIDSet* callid_set = getCallIDSet (call_id);

    if (callid_set == NULL)
        return;

    if (callid_set->empty()) {
        // _debug("CallIDSet with ID: \"%s\" is empty!\n", call_id.c_str());
    }

    if (callid_set->size() == 1) {
        CallIDSet::iterator iter_id = callid_set->begin();
        flushByID (*iter_id, call_id);
    } else {
        // _debug("CallIDSet with ID: \"%s\" is a conference!\n", call_id.c_str());
        CallIDSet::iterator iter_id;

        for (iter_id = callid_set->begin(); iter_id != callid_set->end(); iter_id++) {
            flushByID (*iter_id, call_id);
        }
    }

}

void MainBuffer::flushDefault()
{
    ost::MutexLock guard (_mutex);

    flushByID (default_id, default_id);

}


void MainBuffer::flushByID (CallID call_id, CallID reader_id)
{

    RingBuffer* ringbuffer = getRingBuffer (call_id);

    if (ringbuffer != NULL)
        ringbuffer->flush (reader_id);
}


void MainBuffer::flushAllBuffers()
{

    // _debug("-------------------------- flushAllBuffers()\n");

    RingBufferMap::iterator iter_buffer = _ringBufferMap.begin();

    while (iter_buffer != _ringBufferMap.end()) {
        // _debug("--------------------------    flushing: %s\n",iter_buffer->second->getBufferId().c_str());

        iter_buffer->second->flushAll();

        iter_buffer++;
    }
}


void MainBuffer::stateInfo()
{
    _debug ("MainBuffer state info\n");

    CallIDMap::iterator iter_map = _callIDMap.begin();

    while (iter_map != _callIDMap.end()) {
        CallIDSet* id_set = getCallIDSet (iter_map->first);

        std::string dbg_str ("    Buffer: ");

        dbg_str.append (string (iter_map->first.c_str()));
        dbg_str.append (string (" bound to "));

        if (id_set != NULL) {
            CallIDSet::iterator iter_set = id_set->begin();

            while (iter_set != id_set->end()) {
                dbg_str.append (string (iter_set->c_str()));
                dbg_str.append (string (", "));
            }
        }

        _debug ("%s\n", dbg_str.c_str());

        iter_map++;
    }
}
