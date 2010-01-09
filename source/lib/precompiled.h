/* Copyright (C) 2009 Wildfire Games.
 * This file is part of 0 A.D.
 *
 * 0 A.D. is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * 0 A.D. is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with 0 A.D.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * precompiled header. must be the first non-comment part of every
 * source file (VC6..8 requirement).
 */

// some libraries have only a small number of source files, and the
// overhead of including loads of headers here outweighs the improvements to
// incremental rebuild performance.
// they can set MINIMAL_PCH to 1 so we include far fewer headers (but
// still do the global disabling of warnings and include config headers etc),
// or set it to 2 to remove STL headers too (precompiling STL helps performance
// in most non-tiny cases)
#ifndef MINIMAL_PCH
# define MINIMAL_PCH 0
#endif

#define _SECURE_SCL 0

#include "lib/sysdep/compiler.h"	// MSC_VERSION, HAVE_PCH
#include "lib/sysdep/os.h"	// (must come before posix_types.h)

// disable some common and annoying warnings
// (done as soon as possible so that headers below are covered)
#if MSC_VERSION
// .. temporarily disabled W4 (unimportant but ought to be fixed)
# pragma warning(disable:4201)	// nameless struct (Matrix3D)
# pragma warning(disable:4244)	// conversion from uintN to uint8
// .. permanently disabled W4
# pragma warning(disable:4127)	// conditional expression is constant; rationale: see STMT in lib.h.
# pragma warning(disable:4996)	// function is deprecated
# pragma warning(disable:4786)	// identifier truncated to 255 chars
# pragma warning(disable:4351)	// yes, default init of array entries is desired
// .. disabled only for the precompiled headers
# pragma warning(disable:4702)	// unreachable code (frequent in STL)
// .. VS2005 code analysis (very frequent ones)
# if MSC_VERSION >= 1400
#  pragma warning(disable:6011)	// dereferencing NULL pointer
#  pragma warning(disable:6246)	// local declaration hides declaration of the same name in outer scope
# endif
# if ICC_VERSION
#  pragma warning(disable:383)	// value copied to temporary, reference to temporary used
#  pragma warning(disable:981)	// operands are evaluted in unspecified order
#  pragma warning(disable:1418)	// external function definition with no prior declaration (raised for all non-static function templates)
#  pragma warning(disable:1572)	// floating-point equality and inequality comparisons are unreliable
#  pragma warning(disable:1786)	// function is deprecated (disabling 4996 isn't sufficient)
#  pragma warning(disable:1684)	// conversion from pointer to same-sized integral type
# endif
#endif


//
// headers made available everywhere for convenience
//

// (must come before any system headers because it fixes off_t)
#include "lib/posix/posix_types.h"

#include "lib/code_annotation.h"

// (must come before any use of <math.h> due to incompatibility with ICC's mathimf.h)
#if ICC_VERSION
#include <mathimf.h>
#endif

#include "lib/sysdep/arch.h"

#include "lib/lib_api.h"
#include "lib/types.h"

#if !MINIMAL_PCH
#include "lib/sysdep/stl.h"
#include "lib/lib.h"
#include "lib/lib_errors.h"
#include "lib/secure_crt.h"
#include "lib/debug.h"
#endif // !MINIMAL_PCH

// Boost
// .. if this package isn't going to be statically linked, we're better off
// using Boost via DLL. (otherwise, we would have to ensure the exact same
// compiler is used, which is a pain because MSC8, MSC9 and ICC 10 are in use)
#ifndef LIB_STATIC_LINK
# define BOOST_ALL_DYN_LINK
#endif

// the following boost libraries have been included in TR1 and are
// thus deemed usable:
#include <boost/shared_ptr.hpp>
using boost::shared_ptr;
#if !MINIMAL_PCH
// (these ones are used more rarely, so we don't enable them in minimal configurations)
#include <boost/array.hpp>
using boost::array;
#include <boost/mem_fn.hpp>
using boost::mem_fn;
#include <boost/function.hpp>
using boost::function;
#include <boost/bind.hpp>
using boost::bind;
#include "lib/external_libraries/boost_filesystem.h"
#endif // !MINIMAL_PCH

// (this must come after boost and common lib headers)
#include "lib/posix/posix.h"


//
// precompiled headers
//

// if PCHs are supported and enabled, we make an effort to include all
// system headers. otherwise, only a few central headers (e.g. types)
// are pulled in and source files must include all the system headers
// they use. this policy ensures good compile performance whether or not
// PCHs are being used.

#include "lib/config.h"	// CONFIG_ENABLE_PCH
#include "lib/sysdep/compiler.h"	// HAVE_PCH

#if CONFIG_ENABLE_PCH && HAVE_PCH

// anything placed here won't need to be compiled in each translation unit,
// but will cause a complete rebuild if they change.

#if !MINIMAL_PCH
// all new-form C library headers
#include <cassert>
#include <cctype>
#include <cerrno>
#include <cfloat>
//#include <ciso646> // defines e.g. "and" to "&". unnecessary and causes trouble with asm.
#include <climits>
#include <clocale>
#include <cmath>
//#include <csetjmp>	// incompatible with libpng on Debian/Ubuntu
#include <csignal>
#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <cwchar>
#include <cwctype>
#endif // !MINIMAL_PCH

#if MINIMAL_PCH < 2
// all C++98 STL headers
#include <algorithm>
#include <deque>
#include <functional>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <numeric>
#include <queue>
#include <set>
#include <stack>
#include <utility>
#include <vector>
#endif

#if !MINIMAL_PCH
// all other C++98 headers
#include <bitset>
#include <complex>
#include <exception>
#include <fstream>
#include <iomanip>
#include <ios>
#include <iosfwd>
#include <iostream>
#include <istream>
#include <limits>
#include <locale>
#include <new>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <streambuf>
#include <string>
#include <sstream>
#include <typeinfo>
#include <valarray>
#endif // !MINIMAL_PCH

#if !MINIMAL_PCH
// STL extensions
#if GCC_VERSION >= 402 // (see comment in stl.h about GCC versions)
# include <tr1/unordered_map>
# include <tr1/unordered_set>
#elif GCC_VERSION
# include <ext/hash_map>
# include <ext/hash_set>
#else
# include <hash_map>
# include <hash_set>
#endif
#endif // !MINIMAL_PCH

#endif // #if CONFIG_PCH

// restore temporarily-disabled warnings
#if MSC_VERSION
# pragma warning(default:4702)
#endif
