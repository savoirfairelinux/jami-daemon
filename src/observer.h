/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
 *
 *  Author: Guillaume Roguez <Guillaume.Roguez@savoirfairelinux.com>
 *  Author: Adrien Béraud <adrien.beraud@savoirfairelinux.com>
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
 */

#pragma once

#include "noncopyable.h"

#include <cstdlib>
#include <cstdint>
#include <memory>
#include <set>
#include <list>
#include <mutex>
#include <functional>
#include <ciso646> // fix windows compiler bug
#ifndef __DEBUG__ // this is only defined on plugins build for debugging
#include "logger.h"
#endif

namespace jami {

template<typename T>
class Observer;
template<typename T>
class Observable;

/*=== Observable =============================================================*/

template<typename T>
class Observable
{
public:
    Observable()
        : mutex_()
        , observers_()
    {}

    /**
     * @brief ~Observable
     * Detach all observers to avoid making them call this observable when
     * destroyed
     */
    virtual ~Observable()
    {
        std::lock_guard<std::mutex> lk(mutex_);

        for (auto& pobs : priority_observers_) {
            if (auto so = pobs.lock()) {
                so->detached(this);
            }
        }

        for (auto& o : observers_)
            o->detached(this);
    }

    bool attach(Observer<T>* o)
    {
        std::lock_guard<std::mutex> lk(mutex_);
        if (o and observers_.insert(o).second) {
            o->attached(this);
            return true;
        }
        return false;
    }

    void attachPriorityObserver(std::shared_ptr<Observer<T>> o)
    {
        std::lock_guard<std::mutex> lk(mutex_);
        priority_observers_.push_back(o);
        o->attached(this);
    }

    void detachPriorityObserver(Observer<T>* o)
    {
        std::lock_guard<std::mutex> lk(mutex_);
        for (auto it = priority_observers_.begin(); it != priority_observers_.end(); it++) {
            if (auto so = it->lock()) {
                if (so.get() == o) {
                    so->detached(this);
                    priority_observers_.erase(it);
                    return;
                }
            }
        }
    }

    bool detach(Observer<T>* o)
    {
        std::lock_guard<std::mutex> lk(mutex_);
        if (o and observers_.erase(o)) {
            o->detached(this);
            return true;
        }
        return false;
    }

    size_t getObserversCount()
    {
        std::lock_guard<std::mutex> lk(mutex_);
        return observers_.size() + priority_observers_.size();
    }

protected:
    void notify(T data)
    {
        std::lock_guard<std::mutex> lk(mutex_);
        for (auto it = priority_observers_.begin(); it != priority_observers_.end();) {
            if (auto so = it->lock()) {
                it++;
                try {
                    so->update(this, data);
                } catch (std::exception& e) {
#ifndef __DEBUG__
                    JAMI_ERR() << e.what();
#endif
                }
            } else {
                it = priority_observers_.erase(it);
            }
        }

        for (auto observer : observers_) {
            observer->update(this, data);
        }
    }

private:
    NON_COPYABLE(Observable<T>);

protected:
    std::mutex mutex_; // lock observers_
    std::list<std::weak_ptr<Observer<T>>> priority_observers_;
    std::set<Observer<T>*> observers_;
};

template<typename T>
class PublishObservable : public Observable<T>
{
public:
    void publish(T data) { this->notify(data); }
};

/*=== Observer =============================================================*/

template<typename T>
class Observer
{
public:
    virtual ~Observer() {}
    virtual void update(Observable<T>*, const T&) = 0;
    virtual void attached(Observable<T>*) {}
    virtual void detached(Observable<T>*) {}
};

template<typename T>
class FuncObserver : public Observer<T>
{
public:
    using F = std::function<void(const T&)>;
    FuncObserver(F f)
        : f_(f)
    {}
    virtual ~FuncObserver() {}
    void update(Observable<T>*, const T& t) override { f_(t); }

private:
    F f_;
};

/*=== PublishMapSubject ====================================================*/

template<typename T1, typename T2>
class PublishMapSubject : public Observer<T1>, public Observable<T2>
{
public:
    using F = std::function<T2(const T1&)>;

    PublishMapSubject(F f)
        : map_ {f}
    {}

    void update(Observable<T1>*, const T1& t) override { this->notify(map_(t)); }

    /**
     * @brief attached
     * Here we just make sure that the PublishMapSubject is only attached to one
     * Observable at a time.
     * @param srcObs
     */
    virtual void attached(Observable<T1>* srcObs) override
    {
        if (obs_ != nullptr && obs_ != srcObs) {
            obs_->detach(this);
            obs_ = srcObs;
        }
    }

    /**
     * @brief detached
     * Since a MapSubject is only attached to one Observable, when detached
     * We should detach all of it observers
     */
    virtual void detached(Observable<T1>*) override
    {
        std::lock_guard<std::mutex> lk(this->mutex_);
        for (auto& pobs : this->priority_observers_) {
            if (auto so = pobs.lock()) {
                so->detached(this);
            }
        }
        for (auto& o : this->observers_)
            o->detached(this);
    }

    /**
     * @brief ~PublishMapSubject()
     * Detach all observers to avoid making them call this observable when
     * destroyed
     **/
    ~PublishMapSubject() { detached(nullptr); }

private:
    F map_;
    Observable<T1>* obs_ = nullptr;
};

}; // namespace jami
