/*
 *  Copyright (C) 2013-2019 Savoir-faire Linux Inc.
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
#include <iostream>

namespace jami {

template <typename T> class Observer;
template <typename T> class Observable;

/*=== Observable =============================================================*/

template <typename T>
class Observable
{
public:
    Observable() : mutex_(), observers_() {}

    virtual ~Observable() {
        std::lock_guard<std::mutex> lk(mutex_);
        for (auto& o : observers_)
            o->detached(this);
    }

    bool attach(Observer<T>* o) {
        std::lock_guard<std::mutex> lk(mutex_);
        if (o and observers_.insert(o).second) {
            o->attached(this);
            return true;
        }
        return false;
    }

    void attachPriorityObserver(std::shared_ptr<Observer<T>> o) {
        std::lock_guard<std::mutex> lk(mutex_);
        priority_observers_.push_back(o);
        o->attached(this);
    }

    bool detach(Observer<T>* o) {
        std::lock_guard<std::mutex> lk(mutex_);
        if (o and observers_.erase(o)) {
            o->detached(this);
            return true;
        }
        return false;
    }

    int getObserversCount() {
        std::lock_guard<std::mutex> lk(mutex_);
        return observers_.size();
    }

protected:
    void notify(T data) {
        std::lock_guard<std::mutex> lk(mutex_);

//        for(auto& pobs: priority_observers_) {
//            if(auto so = pobs.lock()) {
//                try {
//                   so->update(this,data);
//                } catch (std::exception& e) {
//                    std::cout << e.what() <<  std::endl;
//                }
//            }
//        }

        for(auto it=priority_observers_.begin(); it != priority_observers_.end();) {
            if(auto so = it->lock()){
                try {
                    so->update(this,data);
                } catch (std::exception& e) {
                    std::cout << e.what() <<  std::endl;
                }
                ++it;
            } else {
                it = priority_observers_.erase(it);
            }
        }

        for (auto observer : observers_) {
            if(observer) {
                observer->update(this, data);
            }
        }
    }

private:
    NON_COPYABLE(Observable<T>);

    std::mutex mutex_; // lock observers_
    std::list<std::weak_ptr<Observer<T>>> priority_observers_;
    std::set<Observer<T>*> observers_;
};

/*=== Observer =============================================================*/

template <typename T>
class Observer
{
public:
    virtual ~Observer() {}
    virtual void update(Observable<T>*, const T&) = 0;
    virtual void attached(Observable<T>*) {}
    virtual void detached(Observable<T>*) {}
};


template <typename T>
class FuncObserver : public Observer<T>
{
public:
    using F = std::function<void(const T&)>;
    FuncObserver(F f) : f_(f) {}
    virtual ~FuncObserver() {}
    void update(Observable<T>*, const T& t) override { f_(t); }
private:
    F f_;
};

/*=== PublishMapSubject ====================================================*/

template <typename T1, typename T2, typename T3>
class PublishMapSubject : public Observer<T1> , public Observable<T3> {
public:
    using PreP = std::function<T2 (const T1&)>;
    using F = std::function<T3(T2&)>;
    using PostP = std::function<void (const T1&, T2&, T3&)>;

    PublishMapSubject(PreP prep, F f, PostP postp) : preprocess_{prep}, map_{f}, postprocess_{postp} {}

    void update(Observable<T1>*, const T1& t) override {
        T2 tpreprocessed = preprocess_(t);
        T3 tmapped = map_(tpreprocessed);
        this->notify(tmapped);
        postprocess_(t, tpreprocessed, tmapped);
    }

private:
    PreP preprocess_;
    F map_;
    PostP postprocess_;
};

}; // namespace jami
