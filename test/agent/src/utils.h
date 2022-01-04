/*
 *  Copyright (C) 2021-2022 Savoir-faire Linux Inc.
 *
 *  Author: Olivier Dion <olivier.dion@savoirfairelinux.com>
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

#include "logger.h"

#define AGENT_ERR(FMT, ARGS...)  JAMI_ERR("AGENT: " FMT, ##ARGS)
#define AGENT_INFO(FMT, ARGS...) JAMI_INFO("AGENT: " FMT, ##ARGS)
#define AGENT_DBG(FMT, ARGS...)  JAMI_DBG("AGENT: " FMT, ##ARGS)
#define AGENT_ASSERT(COND, MSG, ARGS...) \
        if (not(COND)) {                 \
                AGENT_ERR(MSG, ##ARGS);  \
                exit(1);                 \
        }

static inline SCM
to_guile(bool b)
{
    return scm_from_bool(b);
}

static inline SCM
to_guile(const std::string& str)
{
    return scm_from_utf8_string(str.c_str());
}

/* Forward declarations since we call to_guile() recursively for containers */
template<typename T> static inline SCM to_guile(const std::vector<T>& values);
template<typename K, typename V> static inline SCM to_guile(const std::map<K, V>& map);

template<typename T>
static inline SCM
to_guile(const std::vector<T>& values)
{
    SCM vec = scm_c_make_vector(values.size(), SCM_UNDEFINED);

    for (size_t i = 0; i < values.size(); ++i) {
        SCM_SIMPLE_VECTOR_SET(vec, i, to_guile(values[i]));
    }

    return vec;
}

template<typename K, typename V>
static inline SCM
to_guile(const std::map<K, V>& map)
{
    SCM assoc = SCM_EOL;

    for (auto const& [key, value] : map) {
        SCM pair = scm_cons(to_guile(key), to_guile(value));
        assoc = scm_cons(pair, assoc);
    }

    return assoc;
}

template<typename... Args>
static inline SCM
pack_to_guile(Args... args)
{
    SCM lst = SCM_EOL;
    std::vector<SCM> values = {to_guile(args)...};

    while (values.size()) {
        lst = scm_cons(values.back(), lst);
        values.pop_back();
    }

    return lst;
}

template<typename... Args>
static inline SCM
apply_to_guile(SCM body_proc, Args... args)
{
    AGENT_ASSERT(scm_is_true(scm_procedure_p(body_proc)),
                 "body_proc must be a procedure");

    SCM arglst = pack_to_guile(args...);

    return scm_apply_0(body_proc, arglst);
}

struct from_guile
{
    SCM value;
    const char* expr;
    const char* file;
    int line;

    from_guile(SCM val, const char* expr_, const char* file_, int line_)
        : value(val),
          expr(expr_),
          file(file_),
          line(line_)
    {}

    template<typename Pred>
    void ensure_type(const char* msg, Pred&& pred)
    {
        if (!pred(value)) {
            jami::Logger::log(LOG_ERR,
                              file,
                              line,
                              false,
                              "[GUILE] For expression `%s`: "
                              "Scheme value must be of type %s\n",
                              expr,
                              msg);
            exit(EXIT_FAILURE);
        }
    }

    operator bool()
    {
        ensure_type("bool", scm_is_bool);

        return scm_to_bool(value);
    }

    operator uint8_t()
    {
        ensure_type("uint8", [](SCM v){ return scm_is_unsigned_integer(v, 0, 255); });

        return scm_to_int(value);
    }

    operator int()
    {
        ensure_type("integer", scm_is_integer);

        return scm_to_int(value);
    }

    operator std::string()
    {
        ensure_type("string", scm_is_string);

        char* str_raw = scm_to_locale_string(value);
        std::string ret(str_raw);
        free(str_raw);

        return ret;
    }

    template<typename T>
    operator std::vector<T>()
    {
        ensure_type("simple vector", scm_is_simple_vector);

        std::vector<T> ret;

        ret.reserve(SCM_SIMPLE_VECTOR_LENGTH(value));

        for (size_t i = 0; i < SCM_SIMPLE_VECTOR_LENGTH(value); ++i) {
            SCM val = SCM_SIMPLE_VECTOR_REF(value, i);

            ret.emplace_back(from_guile(val, expr, file, line));
        }

        return ret;
    }

    template<typename K, typename V>
    operator std::map<K, V>()
    {
        ensure_type("list", [](SCM v){ return scm_is_true(scm_list_p(v)); });

        std::map<K, V> ret;

        while (not scm_is_null(value)) {
            SCM pair = scm_car(value);

            K key = from_guile(scm_car(pair), expr, file, line);
            V val = from_guile(scm_cdr(pair), expr, file, line);

            ret[key] = val;

            value = scm_cdr(value);
        }

        return ret;
    }
};

#define from_guile(EXPR) from_guile(EXPR, #EXPR, __FILE__, __LINE__)
