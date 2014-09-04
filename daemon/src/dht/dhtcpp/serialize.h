/**
 * Copyright (c) 2013, Simone Pellegrini All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  - Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 *  - Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <vector>
#include <string>
#include <tuple>
#include <numeric>

#include <cassert>

typedef std::vector<uint8_t> StreamType;

template <class T>
inline void serialize(const T&, StreamType&);

namespace detail {

	template<std::size_t> struct int_{};

}

// get_size
template <class T>
size_t get_size(const T& obj);

namespace detail {

	template <class T>
	struct get_size_helper;

	template <class T>
	struct get_size_helper<std::vector<T>> {
		static size_t value(const std::vector<T>& obj) {
		return std::accumulate(obj.begin(), obj.end(), sizeof(size_t),
				[](const size_t& acc, const T& cur) { return acc+get_size(cur); });
		}
	};

	template <>
	struct get_size_helper<std::string> {
		static size_t value(const std::string& obj) {
			return sizeof(size_t) + obj.length()*sizeof(uint8_t);
		}
	};

	template <class tuple_type>
	inline size_t get_tuple_size(const tuple_type& obj, int_<0>) {
		constexpr size_t idx = std::tuple_size<tuple_type>::value-1;
		return get_size(std::get<idx>(obj));
	}

	template <class tuple_type, size_t pos>
	inline size_t get_tuple_size(const tuple_type& obj, int_<pos>) {
		constexpr size_t idx = std::tuple_size<tuple_type>::value-pos-1;
		size_t acc = get_size(std::get<idx>(obj));

		// recur
		return acc+get_tuple_size(obj, int_<pos-1>());
	}

	template <class ...T>
	struct get_size_helper<std::tuple<T...>> {
		static size_t value(const std::tuple<T...>& obj) {
			return get_tuple_size(obj, int_<sizeof...(T)-1>());
		}
	};

	template <class T>
	struct get_size_helper {
		static size_t value(const T&) { return sizeof(T); }
	};

}

template <class T>
inline size_t get_size(const T& obj) {
	return detail::get_size_helper<T>::value(obj);
}

namespace detail {

	template <class T>
	class serialize_helper;

	template <class T>
	void serializer(const T& obj, StreamType::iterator&);

	template <class tuple_type>
	inline void serialize_tuple(const tuple_type& obj, StreamType::iterator& res, int_<0>) {
		constexpr size_t idx = std::tuple_size<tuple_type>::value-1;
		serializer(std::get<idx>(obj), res);
	}

	template <class tuple_type, size_t pos>
	inline void serialize_tuple(const tuple_type& obj, StreamType::iterator& res, int_<pos>) {
		constexpr size_t idx = std::tuple_size<tuple_type>::value-pos-1;
		serializer(std::get<idx>(obj), res);

		// recur
		serialize_tuple(obj, res, int_<pos-1>());
	}

	template <class... T>
	struct serialize_helper<std::tuple<T...>> {
		static void apply(const std::tuple<T...>& obj, StreamType::iterator& res) {
			detail::serialize_tuple(obj, res, detail::int_<sizeof...(T)-1>());
		}

	};

	template <>
	struct serialize_helper<std::string> {
		static void apply(const std::string& obj, StreamType::iterator& res) {
			// store the number of elements of this vector at the beginning
			serializer(obj.length(), res);
			for(const auto& cur : obj) { serializer(cur, res); }
		}

	};

	template <class T>
	struct serialize_helper<std::vector<T>> {
		static void apply(const std::vector<T>& obj, StreamType::iterator& res) {
			// store the number of elements of this vector at the beginning
			serializer(obj.size(), res);
			for(const auto& cur : obj) { serializer(cur, res); }
		}

	};

	template <class T>
	struct serialize_helper {
		static void apply(const T& obj, StreamType::iterator& res) {
			const uint8_t* ptr = reinterpret_cast<const uint8_t*>(&obj);
			std::copy(ptr,ptr+sizeof(T),res);
			res+=sizeof(T);
		}

	};

	template <class T>
	inline void serializer(const T& obj, StreamType::iterator& res) {
		serialize_helper<T>::apply(obj,res);
	}

} // end detail namespace

template <class T>
inline void serialize(const T& obj, StreamType& res) {

	size_t offset = res.size();
	size_t size = get_size(obj);
	res.resize(res.size() + size);

	StreamType::iterator it = res.begin()+offset;
	detail::serializer(obj,it);
	assert(res.begin() + offset + size == it);
}

namespace detail {

	template <class T>
	struct deserialize_helper;

	template <class T>
	struct deserialize_helper {
		static T apply(StreamType::const_iterator& begin,
							StreamType::const_iterator end) {

			assert(begin+sizeof(T)<=end);
			T val;
			uint8_t* ptr = reinterpret_cast<uint8_t*>(&val);
			std::copy(begin, begin+sizeof(T), ptr);
			begin+=sizeof(T);
			return val;
		}
	};

	template <class T>
	struct deserialize_helper<std::vector<T>> {
		static std::vector<T> apply(StreamType::const_iterator& begin,
											 StreamType::const_iterator end)
		{
			// retrieve the number of elements
			size_t size = deserialize_helper<size_t>::apply(begin,end);

			std::vector<T> vect(size);
			for(size_t i=0; i<size; ++i) {
				vect[i] = std::move(deserialize_helper<T>::apply(begin,end));
			}
			return vect;
		}
	};

	template <>
	struct deserialize_helper<std::string> {
		static std::string apply(StreamType::const_iterator& begin,
										 StreamType::const_iterator end)
		{
			// retrieve the number of elements
			size_t size = deserialize_helper<size_t>::apply(begin,end);

			if (size == 0u) return std::string();
			std::string str(size,'\0');
			for(size_t i=0; i<size; ++i) {
				str.at(i) = deserialize_helper<uint8_t>::apply(begin,end);
			}
			return str;
		}
	};

	template <class tuple_type>
	inline void deserialize_tuple(tuple_type& obj,
								  StreamType::const_iterator& begin,
								  StreamType::const_iterator end, int_<0>) {
		constexpr size_t idx = std::tuple_size<tuple_type>::value-1;
		typedef typename std::tuple_element<idx,tuple_type>::type T;

		std::get<idx>(obj) = std::move(deserialize_helper<T>::apply(begin, end));
	}

	template <class tuple_type, size_t pos>
	inline void deserialize_tuple(tuple_type& obj,
								  StreamType::const_iterator& begin,
								  StreamType::const_iterator end, int_<pos>) {
		constexpr size_t idx = std::tuple_size<tuple_type>::value-pos-1;
		typedef typename std::tuple_element<idx,tuple_type>::type T;
		std::get<idx>(obj) = std::move(deserialize_helper<T>::apply(begin, end));

		// recur
		deserialize_tuple(obj, begin, end, int_<pos-1>());
	}

	template <class... T>
	struct deserialize_helper<std::tuple<T...>> {
		static std::tuple<T...> apply(StreamType::const_iterator& begin,
												StreamType::const_iterator end)
		{
			//return std::make_tuple(deserialize(begin,begin+sizeof(T),T())...);
			std::tuple<T...> ret;
			deserialize_tuple(ret, begin, end, int_<sizeof...(T)-1>());
			return ret;
		}

	};

}

template <class T>
inline T deserialize(StreamType::const_iterator& begin, const StreamType::const_iterator& end) {
	return detail::deserialize_helper<T>::apply(begin, end);
}

template <class T>
inline T deserialize(const StreamType& res) {
	StreamType::const_iterator it = res.begin();
	return deserialize<T>(it, res.end());
}
