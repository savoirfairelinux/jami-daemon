/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010 Savoir-Faire Linux Inc.
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
 *
 *  Additional permission under GNU GPL version 3 section 7:
 *
 *  If you modify this program, or any covered work, by linking or
 *  combining it with the OpenSSL project's OpenSSL library (or a
 *  modified version of that library), containing parts covered by the
 *  terms of the OpenSSL or SSLeay licenses, Savoir-Faire Linux Inc.
 *  grants you additional permission to convey the resulting work.
 *  Corresponding Source for a non-source form of such a combination
 *  shall include the source code for the parts of OpenSSL used as well
 *  as that of the covered work.
 */

#include "mainbuffer.h"

#include "manager.h"

MainBuffer::MainBuffer() : _internalSamplingRate (8000)
{
    mixBuffer = new SFLDataFormat[STATIC_BUFSIZE];
}


MainBuffer::~MainBuffer()
{

    delete [] mixBuffer; mixBuffer = NULL;
}


void MainBuffer::setInternalSamplingRate (int sr)
{
  // ost::MutexLock guard (_mutex);

    if (sr != _internalSamplingRate) {

        _internalSamplingRate = sr;

        flushAllBuffers();

	Manager::instance().audioSamplingRateChanged();

    }
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

    CallIDSet* newCallIDSet = new CallIDSet;

    _callIDMap.insert (pair<CallID, CallIDSet*> (set_id, newCallIDSet));

    return true;

}

bool MainBuffer::removeCallIDSet (CallID set_id)
{


    CallIDSet* callid_set = getCallIDSet (set_id);

    if (callid_set != NULL) {
        if (_callIDMap.erase (set_id) != 0) {
        	delete callid_set; callid_set = NULL;
            return true;
        } else {
            _debug ("removeCallIDSet error while removing callid set %s!", set_id.c_str());
            return false;
        }
    } else {
        _debug ("removeCallIDSet error callid set %s does not exist!", set_id.c_str());
        return false;
    }

}

void MainBuffer::addCallIDtoSet (CallID set_id, CallID call_id)
{

    CallIDSet* callid_set = getCallIDSet (set_id);
    callid_set->insert (call_id);

}

void MainBuffer::removeCallIDfromSet (CallID set_id, CallID call_id)
{

    CallIDSet* callid_set = getCallIDSet (set_id);

    if (callid_set != NULL) {
        if (callid_set->erase (call_id) != 0) {
        } else {
            _debug ("removeCallIDfromSet error while removing callid %s from set %s!", call_id.c_str(), set_id.c_str());
        }
    } else {
        _debug ("removeCallIDfromSet error callid set %s does not exist!", set_id.c_str());
    }
}


RingBuffer* MainBuffer::getRingBuffer (CallID call_id)
{

    RingBufferMap::iterator iter = _ringBufferMap.find (call_id);

    if (iter == _ringBufferMap.end()) {
        // _debug("ringBuffer with ID: \"%s\" doesn't exist! ", call_id.c_str());
        return NULL;
    } else
        return iter->second;
}


RingBuffer* MainBuffer::createRingBuffer (CallID call_id)
{

    RingBuffer* newRingBuffer = new RingBuffer (SIZEBUF, call_id);

    _ringBufferMap.insert (pair<CallID, RingBuffer*> (call_id, newRingBuffer));

    return newRingBuffer;
}


bool MainBuffer::removeRingBuffer (CallID call_id)
{

    RingBuffer* ring_buffer = getRingBuffer (call_id);

    if (ring_buffer != NULL) {
        if (_ringBufferMap.erase (call_id) != 0) {
        	delete ring_buffer;
            return true;
        } else {
            _error ("BufferManager: Error: Fail to delete ringbuffer %s!", call_id.c_str());
            return false;
        }
    } else {
        _debug ("BufferManager: Error: Ringbuffer %s does not exist!", call_id.c_str());
        return true;
    }
}


void MainBuffer::bindCallID (CallID call_id1, CallID call_id2)
{

  // ost::MutexLock guard (_mutex);

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

  // ost::MutexLock guard (_mutex);

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

    // ost::MutexLock guard (_mutex);

    CallIDSet* callid_set = getCallIDSet (call_id);

    if (callid_set == NULL)
        return;

    if (callid_set->empty())
        return;

    CallIDSet temp_set = *callid_set;

    CallIDSet::iterator iter_set = temp_set.begin();

    while (iter_set != temp_set.end()) {
        CallID call_id_in_set = *iter_set;
        unBindCallID (call_id, call_id_in_set);

        iter_set++;
    }

}


int MainBuffer::putData (void *buffer, int toCopy, unsigned short volume, CallID call_id)
{

    // ost::MutexLock guard (_mutex);

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

    // ost::MutexLock guard (_mutex);

    RingBuffer* ringbuffer = getRingBuffer (call_id);

    if (ringbuffer == NULL)
        return 0;
    else
        return ringbuffer->AvailForPut();

}


int MainBuffer::getData (void *buffer, int toCopy, unsigned short volume, CallID call_id)
{
  // ost::MutexLock guard (_mutex);

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

    RingBuffer* ring_buffer = getRingBuffer (call_id);

    if (!ring_buffer) {
        return 0;
    }

    return ring_buffer->Get (buffer, toCopy, volume, reader_id);
}


int MainBuffer::availForGet (CallID call_id)
{

  // ost::MutexLock guard (_mutex);

    CallIDSet* callid_set = getCallIDSet (call_id);

    if (callid_set == NULL)
        return 0;

    if (callid_set->empty()) {
        _debug ("MainBuffer: CallIDSet with ID: \"%s\" is empty!", call_id.c_str());
        return 0;
    }

    if (callid_set->size() == 1) {
        CallIDSet::iterator iter_id = callid_set->begin();

        if ( (call_id != default_id) && (*iter_id == call_id)) {
            _debug ("This problem should not occur since we have %i element", (int) callid_set->size());
        }

        // else
        return availForGetByID (*iter_id, call_id);
    } else {
        // _debug("CallIDSet with ID: \"%s\" is a conference!", call_id.c_str());
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
        _error("MainBuffer: Error: RingBuffer has a readpointer on tiself");
    }

    RingBuffer* ringbuffer = getRingBuffer (call_id);

    if (ringbuffer == NULL) {
        _error("MainBuffer: Error: RingBuffer does not exist");
        return 0;
    } else
        return ringbuffer->AvailForGet (reader_id);

}


int MainBuffer::discard (int toDiscard, CallID call_id)
{
    // _debug("MainBuffer::discard");

    // ost::MutexLock guard (_mutex);

    CallIDSet* callid_set = getCallIDSet (call_id);

    if (callid_set == NULL)
        return 0;

    if (callid_set->empty()) {
        // _debug("CallIDSet with ID: \"%s\" is empty!", call_id.c_str());
        return 0;
    }


    if (callid_set->size() == 1) {

        CallIDSet::iterator iter_id = callid_set->begin();

        return discardByID (toDiscard, *iter_id, call_id);
    } else {


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
  // ost::MutexLock guard (_mutex);

    CallIDSet* callid_set = getCallIDSet (call_id);

    if (callid_set == NULL)
        return;

    if (callid_set->empty()) {
        // _debug("CallIDSet with ID: \"%s\" is empty!", call_id.c_str());
    }

    if (callid_set->size() == 1) {
        CallIDSet::iterator iter_id = callid_set->begin();
        flushByID (*iter_id, call_id);
    } else {

        CallIDSet::iterator iter_id;

        for (iter_id = callid_set->begin(); iter_id != callid_set->end(); iter_id++) {
            flushByID (*iter_id, call_id);
        }
    }

}

void MainBuffer::flushDefault()
{
  // ost::MutexLock guard (_mutex);

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

    RingBufferMap::iterator iter_buffer = _ringBufferMap.begin();

    while (iter_buffer != _ringBufferMap.end()) {

        iter_buffer->second->flushAll();

        iter_buffer++;
    }
}


void MainBuffer::stateInfo()
{
    _debug ("MainBuffer: State info");

    CallIDMap::iterator iter_call = _callIDMap.begin();

    // print each call and bound call ids

    while (iter_call != _callIDMap.end()) {

        std::string dbg_str ("    Call: ");
        dbg_str.append (std::string (iter_call->first.c_str()));
        dbg_str.append (std::string ("   is bound to: "));

        CallIDSet* call_id_set = (CallIDSet*) iter_call->second;

        CallIDSet::iterator iter_call_id = call_id_set->begin();

        while (iter_call_id != call_id_set->end()) {

            dbg_str.append (std::string (*iter_call_id));
            dbg_str.append (std::string (", "));

            iter_call_id++;
        }

        _debug ("%s", dbg_str.c_str());

        iter_call++;
    }

    // Print ringbuffers ids and readpointers
    RingBufferMap::iterator iter_buffer = _ringBufferMap.begin();

    while (iter_buffer != _ringBufferMap.end()) {

        RingBuffer* rbuffer = (RingBuffer*) iter_buffer->second;
        ReadPointer* rpointer = NULL;

        std::string dbg_str ("    Buffer: ");

        dbg_str.append (std::string (iter_buffer->first.c_str()));
        dbg_str.append (std::string ("   as read pointer: "));

        if (rbuffer)
            rpointer = rbuffer->getReadPointerList();

        if (rpointer) {

            ReadPointer::iterator iter_pointer = rpointer->begin();

            while (iter_pointer != rpointer->end()) {

                dbg_str.append (string (iter_pointer->first.c_str()));
                dbg_str.append (string (", "));

                iter_pointer++;
            }
        }

        _debug ("%s", dbg_str.c_str());

        iter_buffer++;
    }




}
