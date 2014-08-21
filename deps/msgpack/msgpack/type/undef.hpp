//
// MessagePack for C++ static resolution routine
//
// Copyright (C) 2008-2009 FURUHASHI Sadayuki
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//
//    Unless required by applicable law or agreed to in writing, software
//    distributed under the License is distributed on an "AS IS" BASIS,
//    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//    See the License for the specific language governing permissions and
//    limitations under the License.
//
#ifndef MSGPACK_TYPE_UNDEF_HPP__
#define MSGPACK_TYPE_UNDEF_HPP__

#include "msgpack/object.hpp"

namespace msgpack {

namespace type {

struct undef { };

}  // namespace type


inline type::undef& operator>> (object o, type::undef& v)
{
	if(o.type != type::UNDEF) { throw type_error(); }
	return v;
}

template <typename Stream>
inline packer<Stream>& operator<< (packer<Stream>& o, const type::undef& v)
{
	o.pack_undef();
	return o;
}

inline void operator<< (object& o, type::undef v)
{
	o.type = type::UNDEF;
}

inline void operator<< (object::with_zone& o, type::undef v)
	{ static_cast<object&>(o) << v; }

}  // namespace msgpack

#endif /* msgpack/type/undef.hpp */

