/*
 *  Copyright (C) 2004-2019 Savoir-faire Linux Inc.
 *
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

#ifndef RW_MUTEX_H_
#define RW_MUTEX_H_

#include "noncopyable.h"

#include <mutex>
#include <atomic>
#include <condition_variable>
#include <string>
#include <sstream>

namespace jami {

/**
 * rw_mutex is a shared mutex meant to protect
 * rarely-modified, often-read data structures.
 *
 * Its goal is to optimize read throughput and latency.
 * Multiple threads can concurrently read data while
 * a writer thread gets exclusive access when needed.
 */
class rw_mutex {
	public:
		rw_mutex() : mutex(), canRead(), canWrite(), readers(0), writing(false) {}
		void read_enter() {
			std::unique_lock<std::mutex> lck(mutex);
			canRead.wait(lck, [this]() { return !writing; });
			readers++;
		}
		void read_exit() {
			//std::lock_guard<std::mutex> lck(mutex);
			readers--;
			canWrite.notify_one();
		}
		void write_enter() {
			std::unique_lock<std::mutex> lck(mutex);
			canWrite.wait(lck, [this]() { return !writing && readers==0; });
			writing = true;
		}
		void write_exit() {
			std::lock_guard<std::mutex> lck(mutex);
			writing = false;
			canWrite.notify_one();
			canRead.notify_all();
		}

		struct read_lock {
			public:
				read_lock(rw_mutex& m) : sem(m) {
					sem.read_enter();
				}
				~read_lock() {
					sem.read_exit();
				}
			private:
				rw_mutex& sem;
		};

		struct write_lock {
			public:
				write_lock(rw_mutex& m) : sem(m) {
					sem.write_enter();
				}
				~write_lock() {
					sem.write_exit();
				}
			private:
				rw_mutex& sem;
		};

		read_lock read() {
			return read_lock(*this);
		}

		write_lock write() {
			return write_lock(*this);
		}

		std::string toString() {
			std::stringstream ss;
			ss << "[rw_mutex write:" << (writing?"LOCKED":"unlocked") << " read:" << readers << "]";
			return ss.str();
		}

	private:
		NON_COPYABLE(rw_mutex);
		std::mutex mutex;
		std::condition_variable canRead, canWrite;
		std::atomic<unsigned> readers;
		bool writing;
};

} // namespace jami

#endif
