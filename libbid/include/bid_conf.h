/******************************************************************************
  Copyright (c) 2007-2018, Intel Corp.
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of Intel Corporation nor the names of its contributors
      may be used to endorse or promote products derived from this software
      without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
  THE POSSIBILITY OF SUCH DAMAGE.
******************************************************************************/

#if defined(__cplusplus)
#define BID_EXTERN_C extern "C"
#else
#define BID_EXTERN_C extern
#endif

#ifndef _BID_CONF_H
#define _BID_CONF_H

///////////////////////////////////////////////////////////////
#ifdef IN_LIBGCC2
#if !defined ENABLE_DECIMAL_BID_FORMAT || !ENABLE_DECIMAL_BID_FORMAT
#error BID not enabled in libbid
#endif

#ifndef BID_BIG_ENDIAN
#define BID_BIG_ENDIAN LIBGCC2_FLOAT_WORDS_BIG_ENDIAN
#endif

#ifndef BID_THREAD
#if defined (HAVE_CC_TLS) && defined (USE_TLS)
#define BID_THREAD __thread
#endif
#endif

#define BID__intptr_t_defined
#define DECIMAL_CALL_BY_REFERENCE 0
#define DECIMAL_GLOBAL_ROUNDING 1
#define DECIMAL_GLOBAL_ROUNDING_ACCESS_FUNCTIONS 1
#define DECIMAL_GLOBAL_EXCEPTION_FLAGS 1
#define DECIMAL_GLOBAL_EXCEPTION_FLAGS_ACCESS_FUNCTIONS 1
#define BID_HAS_GCC_DECIMAL_INTRINSICS 1
#endif /* IN_LIBGCC2 */

// Configuration Options

#define DECIMAL_TINY_DETECTION_AFTER_ROUNDING 0
#define BINARY_TINY_DETECTION_AFTER_ROUNDING 1

#define BID_SET_STATUS_FLAGS

#ifndef BID_THREAD
#if defined (_MSC_VER) //Windows
#define BID_THREAD __declspec(thread)
#else
#if !defined(__APPLE__) //Linux, FreeBSD
#define BID_THREAD __thread
#else //Mac OSX, TBD
#define BID_THREAD
#endif //Linux or Mac
#endif //Windows
#endif //BID_THREAD


#ifndef BID_HAS_GCC_DECIMAL_INTRINSICS
#define BID_HAS_GCC_DECIMAL_INTRINSICS 0
#endif

// set sizeof (long) here, for bid32_lrint(), bid64_lrint(), bid128_lrint(),
// and for bid32_lround(), bid64_lround(), bid128_lround()
#ifndef BID_SIZE_LONG
#if defined(WINDOWS)
#define BID_SIZE_LONG 4
#else
#if defined(__x86_64__) || defined (__ia64__)  || defined(HPUX_OS_64)
#define BID_SIZE_LONG 8
#else
#define BID_SIZE_LONG 4
#endif
#endif
#endif

#if !defined(WINDOWS) || defined(__INTEL_COMPILER)
// #define UNCHANGED_BINARY_STATUS_FLAGS
#endif
// #define HPUX_OS

// If DECIMAL_CALL_BY_REFERENCE is defined then numerical arguments and results
// are passed by reference otherwise they are passed by value (except that
// a pointer is always passed to the status flags)

#ifndef DECIMAL_CALL_BY_REFERENCE
#define DECIMAL_CALL_BY_REFERENCE 0
#endif

// If DECIMAL_GLOBAL_ROUNDING is defined then the rounding mode is a global
// variable _IDEC_glbround, otherwise it is passed as a parameter when needed

#ifndef DECIMAL_GLOBAL_ROUNDING
#define DECIMAL_GLOBAL_ROUNDING 1
#endif

#ifndef DECIMAL_GLOBAL_ROUNDING_ACCESS_FUNCTIONS
#define DECIMAL_GLOBAL_ROUNDING_ACCESS_FUNCTIONS 0
#endif

// If DECIMAL_GLOBAL_EXCEPTION_FLAGS is defined then the exception status flags
// are represented by a global variable _IDEC_glbflags, otherwise they are
// passed as a parameter when needed

#ifndef DECIMAL_GLOBAL_EXCEPTION_FLAGS
#define DECIMAL_GLOBAL_EXCEPTION_FLAGS 0
#endif

#ifndef DECIMAL_GLOBAL_EXCEPTION_FLAGS_ACCESS_FUNCTIONS
#define DECIMAL_GLOBAL_EXCEPTION_FLAGS_ACCESS_FUNCTIONS 0
#endif

// If DECIMAL_ALTERNATE_EXCEPTION_HANDLING is defined then the exception masks
// are examined and exception handling information is provided to the caller
// if alternate exception handling is necessary

#ifndef DECIMAL_ALTERNATE_EXCEPTION_HANDLING
#define DECIMAL_ALTERNATE_EXCEPTION_HANDLING 0
#endif

typedef unsigned int _IDEC_round;
typedef unsigned int _IDEC_flags;       // could be a struct with diagnostic info

#if DECIMAL_ALTERNATE_EXCEPTION_HANDLING
  // If DECIMAL_GLOBAL_EXCEPTION_MASKS is defined then the exception mask bits
  // are represented by a global variable _IDEC_exceptionmasks, otherwise they
  // are passed as a parameter when needed; DECIMAL_GLOBAL_EXCEPTION_MASKS is
  // ignored
  // if DECIMAL_ALTERNATE_EXCEPTION_HANDLING is not defined
  // **************************************************************************
#define DECIMAL_GLOBAL_EXCEPTION_MASKS 0
  // **************************************************************************

  // If DECIMAL_GLOBAL_EXCEPTION_INFO is defined then the alternate exception
  // handling information is represented by a global data structure
  // _IDEC_glbexcepthandling, otherwise it is passed by reference as a
  // parameter when needed; DECIMAL_GLOBAL_EXCEPTION_INFO is ignored
  // if DECIMAL_ALTERNATE_EXCEPTION_HANDLING is not defined
  // **************************************************************************
#define DECIMAL_GLOBAL_EXCEPTION_INFO 0
  // **************************************************************************
#endif

// Notes: 1) rnd_mode from _RND_MODE_ARG is used by the caller of a function
//           from this library, and can be any name
//        2) rnd_mode and prnd_mode from _RND_MODE_PARAM are fixed names
//           and *must* be used in the library functions
//        3) _IDEC_glbround is the fixed name for the global variable holding
//           the rounding mode

#if !DECIMAL_GLOBAL_ROUNDING
#if DECIMAL_CALL_BY_REFERENCE
#define _RND_MODE_ARG , &rnd_mode
#define _RND_MODE_PARAM , _IDEC_round *prnd_mode
#define _RND_MODE_PARAM_0 _IDEC_round *prnd_mode
#define _RND_MODE_ARG_ALONE &rnd_mode
#define _RND_MODE_PARAM_ALONE _IDEC_round *prnd_mode
#else
#define _RND_MODE_ARG , rnd_mode
#define _RND_MODE_PARAM , _IDEC_round rnd_mode
#define _RND_MODE_PARAM_0 _IDEC_round rnd_mode
#define _RND_MODE_ARG_ALONE rnd_mode
#define _RND_MODE_PARAM_ALONE _IDEC_round rnd_mode
#endif
#else
#define _RND_MODE_ARG
#define _RND_MODE_PARAM
#define _RND_MODE_ARG_ALONE
#define _RND_MODE_PARAM_ALONE
#define rnd_mode _IDEC_glbround
#endif

// Notes: 1) pfpsf from _EXC_FLAGS_ARG is used by the caller of a function
//           from this library, and can be any name
//        2) pfpsf from _EXC_FLAGS_PARAM is a fixed name and *must* be used
//           in the library functions
//        3) _IDEC_glbflags is the fixed name for the global variable holding
//           the floating-point status flags
#if !DECIMAL_GLOBAL_EXCEPTION_FLAGS
#define _EXC_FLAGS_ARG , pfpsf
#define _EXC_FLAGS_PARAM , _IDEC_flags *pfpsf
#else
#define _EXC_FLAGS_ARG
#define _EXC_FLAGS_PARAM
#define pfpsf &_IDEC_glbflags
#endif

#if DECIMAL_GLOBAL_ROUNDING
BID_EXTERN_C BID_THREAD _IDEC_round _IDEC_glbround;
#endif

#if DECIMAL_GLOBAL_EXCEPTION_FLAGS
BID_EXTERN_C BID_THREAD _IDEC_flags _IDEC_glbflags;
#endif

#if DECIMAL_ALTERNATE_EXCEPTION_HANDLING
#if DECIMAL_GLOBAL_EXCEPTION_MASKS
BID_EXTERN_C BID_THREAD _IDEC_exceptionmasks _IDEC_glbexceptionmasks;
#endif
#if DECIMAL_GLOBAL_EXCEPTION_INFO
BID_EXTERN_C BID_THREAD _IDEC_excepthandling _IDEC_glbexcepthandling;
#endif
#endif

#if DECIMAL_ALTERNATE_EXCEPTION_HANDLING

  // Notes: 1) exc_mask from _EXC_MASKS_ARG is used by the caller of a function
  //           from this library, and can be any name
  //        2) exc_mask and pexc_mask from _EXC_MASKS_PARAM are fixed names
  //           and *must* be used in the library functions
  //        3) _IDEC_glbexceptionmasks is the fixed name for the global
  //           variable holding the floating-point exception masks
#if !DECIMAL_GLOBAL_EXCEPTION_MASKS
#if DECIMAL_CALL_BY_REFERENCE
#define _EXC_MASKS_ARG , &exc_mask
#define _EXC_MASKS_PARAM , _IDEC_exceptionmasks *pexc_mask
#else
#define _EXC_MASKS_ARG , exc_mask
#define _EXC_MASKS_PARAM , _IDEC_exceptionmasks exc_mask
#endif
#else
#define _EXC_MASKS_ARG
#define _EXC_MASKS_PARAM
#define exc_mask _IDEC_glbexceptionmasks
#endif

  // Notes: 1) BID_pexc_info from _EXC_INFO_ARG is used by the caller of a function
  //           from this library, and can be any name
  //        2) BID_pexc_info from _EXC_INFO_PARAM is a fixed name and *must* be
  //           used in the library functions
  //        3) _IDEC_glbexcepthandling is the fixed name for the global
  //           variable holding the floating-point exception information
#if !DECIMAL_GLOBAL_EXCEPTION_INFO
#define _EXC_INFO_ARG , BID_pexc_info
#define _EXC_INFO_PARAM , _IDEC_excepthandling *BID_pexc_info
#else
#define _EXC_INFO_ARG
#define _EXC_INFO_PARAM
#define BID_pexc_info &_IDEC_glbexcepthandling
#endif
#else
#define _EXC_MASKS_ARG
#define _EXC_MASKS_PARAM
#define _EXC_INFO_ARG
#define _EXC_INFO_PARAM
#endif

#ifndef BID_BIG_ENDIAN
#define BID_BIG_ENDIAN 0
#endif

#if BID_BIG_ENDIAN
#define BID_SWAP128(x) {  \
  BID_UINT64 sw;              \
  sw = (x).w[1];          \
  (x).w[1] = (x).w[0];    \
  (x).w[0] = sw;          \
  }
#else
#define BID_SWAP128(x)
#endif

#if DECIMAL_CALL_BY_REFERENCE
#define BID_RETURN_VAL(x) { BID_OPT_RESTORE_BINARY_FLAGS()  *pres = (x); return; }
#if BID_BIG_ENDIAN && defined BID_128RES
#define BID_RETURN(x) { BID_OPT_RESTORE_BINARY_FLAGS()  BID_SWAP128(x); *pres = (x); return; }
#define BID_RETURN_NOFLAGS(x) { BID_SWAP128(x); *pres = (x); return; }
#else
#define BID_RETURN(x) { BID_OPT_RESTORE_BINARY_FLAGS()  *pres = (x); return; }
#define BID_RETURN_NOFLAGS(x) { *pres = (x); return; }
#endif
#else
#define BID_RETURN_VAL(x) { BID_OPT_RESTORE_BINARY_FLAGS()  return(x); }
#if BID_BIG_ENDIAN && defined BID_128RES
#define BID_RETURN(x) { BID_OPT_RESTORE_BINARY_FLAGS()  BID_SWAP128(x); return(x); }
#define BID_RETURN_NOFLAGS(x) { BID_SWAP128(x); return(x); }
#else
#define BID_RETURN(x) { BID_OPT_RESTORE_BINARY_FLAGS()  return(x); }
#define BID_RETURN_NOFLAGS(x) { return(x); }
#endif
#endif

#if DECIMAL_CALL_BY_REFERENCE
#define BIDECIMAL_CALL1(_FUNC, _RES, _OP1) \
    _FUNC(&(_RES), &(_OP1) _RND_MODE_ARG _EXC_FLAGS_ARG _EXC_MASKS_ARG _EXC_INFO_ARG)
#define BIDECIMAL_CALL1_NORND(_FUNC, _RES, _OP1) \
    _FUNC(&(_RES), &(_OP1) _EXC_FLAGS_ARG _EXC_MASKS_ARG _EXC_INFO_ARG)
#define BIDECIMAL_CALL2(_FUNC, _RES, _OP1, _OP2) \
    _FUNC(&(_RES), &(_OP1), &(_OP2) _RND_MODE_ARG _EXC_FLAGS_ARG _EXC_MASKS_ARG _EXC_INFO_ARG)
#define BIDECIMAL_CALL2_YPTR_NORND(_FUNC, _RES, _OP1, _OP2) \
    _FUNC(&(_RES), &(_OP1), &(_OP2) _EXC_FLAGS_ARG _EXC_MASKS_ARG _EXC_INFO_ARG)
#define BIDECIMAL_CALL2_NORND(_FUNC, _RES, _OP1, _OP2) \
    _FUNC(&(_RES), &(_OP1), &(_OP2) _EXC_FLAGS_ARG _EXC_MASKS_ARG _EXC_INFO_ARG)
#define BIDECIMAL_CALL1_NORND_RESREF(_FUNC, _RES, _OP1) \
    _FUNC((_RES), &(_OP1) _EXC_FLAGS_ARG _EXC_MASKS_ARG _EXC_INFO_ARG)
#define BIDECIMAL_CALL1_RESARG(_FUNC, _RES, _OP1) \
    _FUNC(&(_RES), (_OP1) _RND_MODE_ARG _EXC_FLAGS_ARG _EXC_MASKS_ARG _EXC_INFO_ARG)
#define BIDECIMAL_CALL1_RESREF(_FUNC, _RES, _OP1) \
    _FUNC((_RES), &(_OP1) _RND_MODE_ARG _EXC_FLAGS_ARG _EXC_MASKS_ARG _EXC_INFO_ARG)
#define BIDECIMAL_CALL1_NORND_NOSTAT(_FUNC, _RES, _OP1) \
    _FUNC(&(_RES), &(_OP1) _EXC_MASKS_ARG _EXC_INFO_ARG)
#define BIDECIMAL_CALL2_NORND_NOSTAT(_FUNC, _RES, _OP1, _OP2) \
    _FUNC(&(_RES), &(_OP1), &(_OP2) _EXC_MASKS_ARG _EXC_INFO_ARG)
#define BIDECIMAL_CALL3(_FUNC, _RES, _OP1, _OP2, _OP3) \
    _FUNC(&(_RES), &(_OP1), &(_OP2), &(_OP3) _RND_MODE_ARG _EXC_FLAGS_ARG _EXC_MASKS_ARG _EXC_INFO_ARG)
#define BIDECIMAL_CALL1_NORND_NOMASK_NOINFO(_FUNC, _RES, _OP1) \
    _FUNC(&(_RES), &(_OP1) _EXC_FLAGS_ARG )
#define BIDECIMAL_CALL1_NORND_NOFLAGS_NOMASK_NOINFO(_FUNC, _RES, _OP1) \
    _FUNC(&(_RES), &(_OP1) )
#define BIDECIMAL_CALL1_NORND_NOFLAGS_NOMASK_NOINFO_ARGREF(_FUNC, _RES, _OP1) \
    _FUNC(&(_RES), (_OP1) )
#define BIDECIMAL_CALL2_NORND_NOFLAGS_NOMASK_NOINFO(_FUNC, _RES, _OP1, _OP2) \
    _FUNC(&(_RES), &(_OP1), &(_OP2) )
#define BIDECIMAL_CALL2_NORND_NOFLAGS_NOMASK_NOINFO_ARG2REF(_FUNC, _RES, _OP1, _OP2) \
    _FUNC(&(_RES), &(_OP1), (_OP2) )
#define BIDECIMAL_CALL1_NORND_NOMASK_NOINFO_RESVOID(_FUNC, _OP1) \
    _FUNC(&(_OP1) _EXC_FLAGS_ARG )
#define BIDECIMAL_CALL2_NORND_NOMASK_NOINFO_RESVOID(_FUNC, _OP1, _OP2) \
    _FUNC(&(_OP1), &(_OP2) _EXC_FLAGS_ARG )
#define BIDECIMAL_CALLV_NOFLAGS_NOMASK_NOINFO(_FUNC, _RES) \
    _FUNC(&(_RES) _RND_MODE_ARG)
#define BIDECIMAL_CALL1_NOFLAGS_NOMASK_NOINFO(_FUNC, _RES, _OP1) \
    _FUNC(&(_OP1) _RND_MODE_ARG)
#define BIDECIMAL_CALLV_EMPTY(_FUNC, _RES) \
    _FUNC(&(_RES))
#else
#define BIDECIMAL_CALL1(_FUNC, _RES, _OP1) \
    _RES = _FUNC((_OP1) _RND_MODE_ARG _EXC_FLAGS_ARG _EXC_MASKS_ARG _EXC_INFO_ARG)
#define BIDECIMAL_CALL1_NORND(_FUNC, _RES, _OP1) \
    _RES = _FUNC((_OP1) _EXC_FLAGS_ARG _EXC_MASKS_ARG _EXC_INFO_ARG)
#define BIDECIMAL_CALL2(_FUNC, _RES, _OP1, _OP2) \
    _RES = _FUNC((_OP1), (_OP2) _RND_MODE_ARG _EXC_FLAGS_ARG _EXC_MASKS_ARG _EXC_INFO_ARG)
#define BIDECIMAL_CALL2_YPTR_NORND(_FUNC, _RES, _OP1, _OP2) \
    _RES = _FUNC((_OP1), &(_OP2) _EXC_FLAGS_ARG _EXC_MASKS_ARG _EXC_INFO_ARG)
#define BIDECIMAL_CALL2_NORND(_FUNC, _RES, _OP1, _OP2) \
    _RES = _FUNC((_OP1), (_OP2) _EXC_FLAGS_ARG _EXC_MASKS_ARG _EXC_INFO_ARG)
#define BIDECIMAL_CALL1_NORND_RESREF(_FUNC, _RES, _OP1) \
    _FUNC((_RES), _OP1 _EXC_FLAGS_ARG _EXC_MASKS_ARG _EXC_INFO_ARG)
#define BIDECIMAL_CALL1_RESARG(_FUNC, _RES, _OP1) \
    _RES = _FUNC((_OP1) _RND_MODE_ARG _EXC_FLAGS_ARG _EXC_MASKS_ARG _EXC_INFO_ARG)
#define BIDECIMAL_CALL1_RESREF(_FUNC, _RES, _OP1) \
    _FUNC((_RES), _OP1 _RND_MODE_ARG _EXC_FLAGS_ARG _EXC_MASKS_ARG _EXC_INFO_ARG)
#define BIDECIMAL_CALL1_NORND_NOSTAT(_FUNC, _RES, _OP1) \
    _RES = _FUNC((_OP1) _EXC_MASKS_ARG _EXC_INFO_ARG)
#define BIDECIMAL_CALL2_NORND_NOSTAT(_FUNC, _RES, _OP1, _OP2) \
    _RES = _FUNC((_OP1), (_OP2) _EXC_MASKS_ARG _EXC_INFO_ARG)
#define BIDECIMAL_CALL3(_FUNC, _RES, _OP1, _OP2, _OP3) \
    _RES = _FUNC((_OP1), (_OP2), (_OP3) _RND_MODE_ARG _EXC_FLAGS_ARG _EXC_MASKS_ARG _EXC_INFO_ARG)
#define BIDECIMAL_CALL1_NORND_NOMASK_NOINFO(_FUNC, _RES, _OP1) \
    _RES = _FUNC((_OP1) _EXC_FLAGS_ARG)
#define BIDECIMAL_CALL1_NORND_NOFLAGS_NOMASK_NOINFO(_FUNC, _RES, _OP1) \
    _RES = _FUNC((_OP1) )
#define BIDECIMAL_CALL1_NORND_NOFLAGS_NOMASK_NOINFO_ARGREF(_FUNC, _RES, _OP1) \
    _RES = _FUNC((_OP1) )
#define BIDECIMAL_CALL2_NORND_NOFLAGS_NOMASK_NOINFO(_FUNC, _RES, _OP1, _OP2) \
    _RES = _FUNC((_OP1), (_OP2) )
#define BIDECIMAL_CALL2_NORND_NOFLAGS_NOMASK_NOINFO_ARG2REF(_FUNC, _RES, _OP1, _OP2) \
    _RES = _FUNC((_OP1), (_OP2) )
#define BIDECIMAL_CALL1_NORND_NOMASK_NOINFO_RESVOID(_FUNC, _OP1) \
    _FUNC((_OP1) _EXC_FLAGS_ARG)
#define BIDECIMAL_CALL2_NORND_NOMASK_NOINFO_RESVOID(_FUNC, _OP1, _OP2) \
    _FUNC((_OP1), (_OP2) _EXC_FLAGS_ARG)
#define BIDECIMAL_CALLV_NOFLAGS_NOMASK_NOINFO(_FUNC, _RES) \
    _RES = _FUNC(_RND_MODE_ARG_ALONE)
#if !DECIMAL_GLOBAL_ROUNDING
#define BIDECIMAL_CALL1_NOFLAGS_NOMASK_NOINFO(_FUNC, _RES, _OP1) \
    _RES = _FUNC((_OP1) _RND_MODE_ARG)
#else
#define BIDECIMAL_CALL1_NOFLAGS_NOMASK_NOINFO(_FUNC, _RES, _OP1) \
    _FUNC((_OP1) _RND_MODE_ARG)
#endif
#define BIDECIMAL_CALLV_EMPTY(_FUNC, _RES) \
    _RES=_FUNC()
#endif



///////////////////////////////////////////////////////////////////////////
//
//  Wrapper macros for ICL
//
///////////////////////////////////////////////////////////////////////////

#if defined (__INTEL_COMPILER) && (__DFP_WRAPPERS_ON) && (!DECIMAL_CALL_BY_REFERENCE) && (DECIMAL_GLOBAL_ROUNDING) && (DECIMAL_GLOBAL_EXCEPTION_FLAGS)

#include "bid_wrap_names.h"

#define DECLSPEC_OPT __declspec(noinline)

#define bit_size_BID_UINT128 128
#define bit_size_BID_UINT64 64
#define bit_size_BID_SINT64 64
#define bit_size_BID_UINT32 32
#define bit_size_BID_SINT32 32

#define bidsize(x) bit_size_##x

#define form_type(type, size)  type##size

#define wrapper_name(x)  __wrap_##x

#define DFP_WRAPFN_OTHERTYPE(rsize, fn_name, othertype)\
	form_type(_Decimal,rsize) wrapper_name(fn_name) (othertype __wraparg1)\
{\
union {\
   form_type(_Decimal, rsize) d;\
   form_type(BID_UINT, rsize) i;\
   } r;\
   \
   \
   r.i = fn_name(__wraparg1);\
   return r.d;\
}

#define DFP_WRAPFN_DFP(rsize, fn_name, isize1)\
	form_type(_Decimal,rsize) wrapper_name(fn_name) (form_type(_Decimal, isize1) __wraparg1)\
{\
union {\
   form_type(_Decimal, rsize) d;\
   form_type(BID_UINT, rsize) i;\
   } r;\
   \
union {\
   form_type(_Decimal, isize1) d;\
   form_type(BID_UINT, isize1) i;\
   } in1 = { __wraparg1};\
   \
   \
   r.i = fn_name(in1.i);\
   \
   return r.d;\
}

#define DFP_WRAPFN_DFP_DFP(rsize, fn_name, isize1, isize2)\
form_type(_Decimal, rsize) wrapper_name(fn_name) (form_type(_Decimal, isize1) __wraparg1, form_type(_Decimal, isize2) __wraparg2)\
{\
union {\
   form_type(_Decimal, rsize) d;\
   form_type(BID_UINT, rsize) i;\
   } r;\
   \
union {\
   form_type(_Decimal, isize1) d;\
   form_type(BID_UINT, isize1) i;\
   } in1 = { __wraparg1};\
      \
union {\
   form_type(_Decimal, isize2) d;\
   form_type(BID_UINT, isize2) i;\
   } in2 = { __wraparg2};\
   \
   \
   r.i = fn_name(in1.i, in2.i);\
   \
   return r.d;\
}

#define DFP_WRAPFN_DFP_DFP_POINTER(rsize, fn_name, isize1, isize2)\
form_type(_Decimal, rsize) wrapper_name(fn_name) (form_type(_Decimal, isize1) __wraparg1, form_type(_Decimal, isize2) *__wraparg2)\
{\
union {\
   form_type(_Decimal, rsize) d;\
   form_type(BID_UINT, rsize) i;\
   } r;\
   \
union {\
   form_type(_Decimal, isize1) d;\
   form_type(BID_UINT, isize1) i;\
   } in1 = { __wraparg1};\
      \
union {\
   form_type(_Decimal, isize2) d;\
   form_type(BID_UINT, isize2) i;\
   } out2;\
   \
   \
   r.i = fn_name(in1.i, &out2.i);   *__wraparg2 = out2.d;\
   \
   return r.d;\
}

#define DFP_WRAPFN_DFP_DFP_DFP(rsize, fn_name, isize1, isize2, isize3)\
form_type(_Decimal, rsize) wrapper_name(fn_name) (form_type(_Decimal, isize1) __wraparg1, form_type(_Decimal, isize2) __wraparg2, form_type(_Decimal, isize3) __wraparg3)\
{\
union {\
   form_type(_Decimal, rsize) d;\
   form_type(BID_UINT, rsize) i;\
   } r;\
   \
union {\
   form_type(_Decimal, isize1) d;\
   form_type(BID_UINT, isize1) i;\
   } in1 = { __wraparg1};\
      \
union {\
   form_type(_Decimal, isize2) d;\
   form_type(BID_UINT, isize2) i;\
   } in2 = { __wraparg2};\
   \
union {\
   form_type(_Decimal, isize3) d;\
   form_type(BID_UINT, isize3) i;\
   } in3 = { __wraparg3};\
   \
   \
   r.i = fn_name(in1.i, in2.i, in3.i);\
   \
   return r.d;\
}

#define RES_WRAPFN_DFP(restype, fn_name, isize1)\
restype wrapper_name(fn_name) (form_type(_Decimal, isize1) __wraparg1)\
{\
union {\
   form_type(_Decimal, isize1) d;\
   form_type(BID_UINT, isize1) i;\
   } in1 = { __wraparg1};\
   \
   \
   return fn_name(in1.i);\
}

#define RES_WRAPFN_DFP_DFP(restype, fn_name, isize1, isize2)\
restype wrapper_name(fn_name) (form_type(_Decimal, isize1) __wraparg1, form_type(_Decimal, isize2) __wraparg2)\
{\
union {\
   form_type(_Decimal, isize1) d;\
   form_type(BID_UINT, isize1) i;\
   } in1 = { __wraparg1};\
      \
union {\
   form_type(_Decimal, isize2) d;\
   form_type(BID_UINT, isize2) i;\
   } in2 = { __wraparg2};\
   \
   \
   return fn_name(in1.i, in2.i);\
}

#define DFP_WRAPFN_DFP_OTHERTYPE(rsize, fn_name, isize1, othertype)\
form_type(_Decimal, rsize) wrapper_name(fn_name) (form_type(_Decimal, isize1) __wraparg1, othertype __wraparg2)\
{\
union {\
   form_type(_Decimal, rsize) d;\
   form_type(BID_UINT, rsize) i;\
   } r;\
   \
union {\
   form_type(_Decimal, isize1) d;\
   form_type(BID_UINT, isize1) i;\
   } in1 = { __wraparg1};\
   \
   \
   r.i = fn_name(in1.i, __wraparg2);\
   \
   return r.d;\
}

#define VOID_WRAPFN_OTHERTYPERES_DFP(fn_name, othertype, isize1)\
void wrapper_name(fn_name) (othertype *__wrapres, form_type(_Decimal, isize1) __wraparg1)\
{\
union {\
   form_type(_Decimal, isize1) d;\
   form_type(BID_UINT, isize1) i;\
   } in1 = { __wraparg1};\
   \
   \
   fn_name(__wrapres, in1.i);\
   \
}

#define DFP_WRAPFN_TYPE1_TYPE2(rsize, fn_name, type1, type2)\
form_type(_Decimal, rsize) wrapper_name(fn_name) (type1 __wraparg1, type2 __wraparg2)\
{\
union {\
   form_type(_Decimal, rsize) d;\
   form_type(BID_UINT, rsize) i;\
   } r;\
   \
   \
   r.i = fn_name(__wraparg1, __wraparg2);\
   \
   return r.d;\
}

#else

#define DECLSPEC_OPT

#define DFP_WRAPFN_OTHERTYPE(rsize, fn_name, othertype)
#define DFP_WRAPFN_DFP(rsize, fn_name, isize1)
#define DFP_WRAPFN_DFP_DFP(rsize, fn_name, isize1, isize2)
#define RES_WRAPFN_DFP(rsize, fn_name, isize1)
#define RES_WRAPFN_DFP_DFP(rsize, fn_name, isize1, isize2)
#define DFP_WRAPFN_DFP_DFP_DFP(rsize, fn_name, isize1, isize2, isize3)
#define DFP_WRAPFN_DFP_OTHERTYPE(rsize, fn_name, isize1, othertype)
#define DFP_WRAPFN_DFP_DFP_POINTER(rsize, fn_name, isize1, isize2)
#define VOID_WRAPFN_OTHERTYPERES_DFP(fn_name, othertype, isize1)
#define DFP_WRAPFN_TYPE1_TYPE2(rsize, fn_name, type1, type2)

#endif


///////////////////////////////////////////////////////////////////////////

#if BID_BIG_ENDIAN
#define BID_HIGH_128W 0
#define BID_LOW_128W  1
#else
#define BID_HIGH_128W 1
#define BID_LOW_128W  0
#endif

#if (BID_BIG_ENDIAN) && defined(BID_128RES)
#define BID_COPY_ARG_REF(arg_name) \
       BID_UINT128 arg_name={{ pbid_##arg_name->w[1], pbid_##arg_name->w[0]}};
#define BID_COPY_ARG_VAL(arg_name) \
       BID_UINT128 arg_name={{ bid_##arg_name.w[1], bid_##arg_name.w[0]}};
#else
#define BID_COPY_ARG_REF(arg_name) \
       BID_UINT128 arg_name=*pbid_##arg_name;
#define BID_COPY_ARG_VAL(arg_name) \
       BID_UINT128 arg_name= bid_##arg_name;
#endif

#define BID_COPY_ARG_TYPE_REF(type, arg_name) \
       type arg_name=*pbid_##arg_name;
#define BID_COPY_ARG_TYPE_VAL(type, arg_name) \
       type arg_name= bid_##arg_name;

#if !DECIMAL_GLOBAL_ROUNDING
#define BID_SET_RND_MODE() \
  _IDEC_round rnd_mode = *prnd_mode;
#else
#define BID_SET_RND_MODE()
#endif

#if !defined(BID_MS_FLAGS) && (defined(_MSC_VER) && !defined(__INTEL_COMPILER))
#   define BID_MS_FLAGS
#endif

#if (defined(_MSC_VER) && !defined(__INTEL_COMPILER))
#   include <math.h>    // needed for MS build of some BID32 transcendentals (hypot)
#endif



#if defined (UNCHANGED_BINARY_STATUS_FLAGS) && defined (BID_FUNCTION_SETS_BINARY_FLAGS)
#   if defined( BID_MS_FLAGS )

#       include <float.h>

        extern unsigned int __bid_ms_restore_flags(unsigned int*);

#       define BID_OPT_FLAG_DECLARE() \
            unsigned int binaryflags = 0;
#       define BID_OPT_SAVE_BINARY_FLAGS() \
             binaryflags = _statusfp();
#        define BID_OPT_RESTORE_BINARY_FLAGS() \
              __bid_ms_restore_flags(&binaryflags);

#   else
#       include <fenv.h>
#       define BID_FE_ALL_FLAGS FE_INVALID|FE_DIVBYZERO|FE_OVERFLOW|FE_UNDERFLOW|FE_INEXACT
#       define BID_OPT_FLAG_DECLARE() \
            fexcept_t binaryflags = 0;
#       define BID_OPT_SAVE_BINARY_FLAGS() \
            (void) fegetexceptflag (&binaryflags, BID_FE_ALL_FLAGS);
#       define BID_OPT_RESTORE_BINARY_FLAGS() \
            (void) fesetexceptflag (&binaryflags, BID_FE_ALL_FLAGS);
#   endif
#else
#   define BID_OPT_FLAG_DECLARE()
#   define BID_OPT_SAVE_BINARY_FLAGS()
#   define BID_OPT_RESTORE_BINARY_FLAGS()
#endif

#define BID_PROLOG_REF(arg_name) \
       BID_COPY_ARG_REF(arg_name)

#define BID_PROLOG_VAL(arg_name) \
       BID_COPY_ARG_VAL(arg_name)

#define BID_PROLOG_TYPE_REF(type, arg_name) \
       BID_COPY_ARG_TYPE_REF(type, arg_name)

#define BID_PROLOG_TYPE_VAL(type, arg_name) \
      BID_COPY_ARG_TYPE_VAL(type, arg_name)

#define OTHER_BID_PROLOG_REF()    BID_OPT_FLAG_DECLARE()
#define OTHER_BID_PROLOG_VAL()    BID_OPT_FLAG_DECLARE()

#if DECIMAL_CALL_BY_REFERENCE
#define       BID128_FUNCTION_ARG1(fn_name, arg_name)\
      void fn_name (BID_UINT128 * pres, \
           BID_UINT128 *  \
           pbid_##arg_name _RND_MODE_PARAM _EXC_FLAGS_PARAM _EXC_MASKS_PARAM \
           _EXC_INFO_PARAM) {\
     BID_PROLOG_REF(arg_name)   \
     BID_SET_RND_MODE()         \
     OTHER_BID_PROLOG_REF()

#define       BID128_FUNCTION_ARG1_NORND(fn_name, arg_name)\
      void fn_name (BID_UINT128 * pres, \
           BID_UINT128 *  \
           pbid_##arg_name _EXC_FLAGS_PARAM _EXC_MASKS_PARAM \
           _EXC_INFO_PARAM) {\
     BID_PROLOG_REF(arg_name)   \
     OTHER_BID_PROLOG_REF()

#define       BID128_FUNCTION_ARG1_NORND_CUSTOMRESTYPE(restype, fn_name, arg_name)\
      void fn_name (restype * pres, \
           BID_UINT128 *  \
           pbid_##arg_name _EXC_FLAGS_PARAM _EXC_MASKS_PARAM \
           _EXC_INFO_PARAM) {\
     BID_PROLOG_REF(arg_name)   \
     OTHER_BID_PROLOG_REF()

#define       BID128_FUNCTION_ARG2(fn_name, arg_name1, arg_name2)\
      void fn_name (BID_UINT128 * pres, \
           BID_UINT128 *pbid_##arg_name1,  BID_UINT128 *pbid_##arg_name2  \
           _RND_MODE_PARAM _EXC_FLAGS_PARAM _EXC_MASKS_PARAM \
           _EXC_INFO_PARAM) {\
     BID_PROLOG_REF(arg_name1)   \
     BID_PROLOG_REF(arg_name2)   \
     BID_SET_RND_MODE()         \
     OTHER_BID_PROLOG_REF()

#define       BID128_FUNCTION_ARG2_NORND(fn_name, arg_name1, arg_name2)\
      void fn_name (BID_UINT128 * pres, \
           BID_UINT128 *pbid_##arg_name1,  BID_UINT128 *pbid_##arg_name2  \
           _EXC_FLAGS_PARAM _EXC_MASKS_PARAM \
           _EXC_INFO_PARAM) {\
     BID_PROLOG_REF(arg_name1)   \
     BID_PROLOG_REF(arg_name2)   \
     OTHER_BID_PROLOG_REF()

#define       BID128_FUNCTION_ARG2_NORND_CUSTOMRESTYPE(restype, fn_name, arg_name1, arg_name2)\
      void fn_name (restype * pres, \
           BID_UINT128 *pbid_##arg_name1,  BID_UINT128 *pbid_##arg_name2  \
           _EXC_FLAGS_PARAM _EXC_MASKS_PARAM \
           _EXC_INFO_PARAM) {\
     BID_PROLOG_REF(arg_name1)   \
     BID_PROLOG_REF(arg_name2)   \
     OTHER_BID_PROLOG_REF()

#define       BID128_FUNCTION_ARG2P_NORND_CUSTOMRESTYPE(restype, fn_name, arg_name1, arg_name2)\
      void fn_name (restype * pres, \
           BID_UINT128 *pbid_##arg_name1,  BID_UINT128 *arg_name2  \
           _EXC_FLAGS_PARAM _EXC_MASKS_PARAM \
           _EXC_INFO_PARAM) {\
     BID_PROLOG_REF(arg_name1)   \
     OTHER_BID_PROLOG_REF()

#define       BID128_FUNCTION_ARG3P_NORND_CUSTOMRESTYPE(restype, fn_name, arg_name1, arg_name2, res_name3)\
      void fn_name (restype * pres, \
           BID_UINT128 *pbid_##arg_name1,  BID_UINT128 *pbid_##arg_name2, BID_UINT128 *res_name3  \
           _EXC_FLAGS_PARAM _EXC_MASKS_PARAM \
           _EXC_INFO_PARAM) {\
     BID_PROLOG_REF(arg_name1)   \
     BID_PROLOG_REF(arg_name2)   \
     OTHER_BID_PROLOG_REF()

#define       BID128_FUNCTION_ARG128_ARGTYPE2(fn_name, arg_name1, type2, arg_name2)\
      void fn_name (BID_UINT128 * pres, \
           BID_UINT128 *pbid_##arg_name1,  type2 *pbid_##arg_name2  \
           _RND_MODE_PARAM _EXC_FLAGS_PARAM _EXC_MASKS_PARAM \
           _EXC_INFO_PARAM) {\
     BID_PROLOG_REF(arg_name1)   \
     BID_PROLOG_TYPE_REF(type2, arg_name2)   \
     BID_SET_RND_MODE()         \
     OTHER_BID_PROLOG_REF()

#define BID128_FUNCTION_ARG128_CUSTOMARGTYPE2(fn_name, arg_name1, type2, arg_name2) BID128_FUNCTION_ARG128_ARGTYPE2(fn_name, arg_name1, type2, arg_name2)

#define       BID128_FUNCTION_ARG128_CUSTOMARGTYPE2_PLAIN(fn_name, arg_name1, type2, arg_name2)\
      void fn_name (BID_UINT128 * pres, \
           BID_UINT128 *pbid_##arg_name1,  type2 arg_name2  \
           ) {\
     BID_PROLOG_REF(arg_name1)   \
     OTHER_BID_PROLOG_REF()

#define       BID_TYPE0_FUNCTION_ARGTYPE1_ARGTYPE2(type0, fn_name, type1, arg_name1, type2, arg_name2)\
      void fn_name (type0 *pres, \
           type1 *pbid_##arg_name1,  type2 *pbid_##arg_name2  \
           _RND_MODE_PARAM _EXC_FLAGS_PARAM _EXC_MASKS_PARAM \
           _EXC_INFO_PARAM) {\
     BID_PROLOG_TYPE_REF(type1, arg_name1)   \
     BID_PROLOG_TYPE_REF(type2, arg_name2)   \
     BID_SET_RND_MODE()         \
     OTHER_BID_PROLOG_REF()

#define       BID_TYPE0_FUNCTION_ARGTYPE1_OTHER_ARGTYPE2  BID_TYPE0_FUNCTION_ARGTYPE1_ARGTYPE2

#define       BID_TYPE0_FUNCTION_ARGTYPE1_ARGTYPE2_ARGTYPE3(type0, fn_name, type1, arg_name1, type2, arg_name2, type3, arg_name3)\
      void fn_name (type0 *pres, \
           type1 *pbid_##arg_name1,  type2 *pbid_##arg_name2,  type3 *pbid_##arg_name3  \
           _RND_MODE_PARAM _EXC_FLAGS_PARAM _EXC_MASKS_PARAM \
           _EXC_INFO_PARAM) {\
     BID_PROLOG_TYPE_REF(type1, arg_name1)   \
     BID_PROLOG_TYPE_REF(type2, arg_name2)   \
     BID_PROLOG_TYPE_REF(type3, arg_name3)   \
     BID_SET_RND_MODE()         \
     OTHER_BID_PROLOG_REF()

#define       BID_TYPE_FUNCTION_ARG2(type0, fn_name, arg_name1, arg_name2)\
      void fn_name (type0 *pres, \
           type0 *pbid_##arg_name1,  type0 *pbid_##arg_name2  \
           _RND_MODE_PARAM _EXC_FLAGS_PARAM _EXC_MASKS_PARAM \
           _EXC_INFO_PARAM) {\
     BID_PROLOG_TYPE_REF(type0, arg_name1)   \
     BID_PROLOG_TYPE_REF(type0, arg_name2)   \
     BID_SET_RND_MODE()         \
     OTHER_BID_PROLOG_REF()

#define       BID_TYPE_FUNCTION_ARG2_CUSTOMRESULT_NORND(typeres, fn_name, type0, arg_name1, arg_name2)\
      void fn_name (typeres *pres, \
           type0 *pbid_##arg_name1,  type0 *pbid_##arg_name2  \
           _EXC_FLAGS_PARAM _EXC_MASKS_PARAM \
           _EXC_INFO_PARAM) {\
     BID_PROLOG_TYPE_REF(type0, arg_name1)   \
     BID_PROLOG_TYPE_REF(type0, arg_name2)   \
     OTHER_BID_PROLOG_REF()

#define       BID_TYPE_FUNCTION_ARG1(type0, fn_name, arg_name1)\
      void fn_name (type0 *pres, \
           type0 *pbid_##arg_name1  \
           _RND_MODE_PARAM _EXC_FLAGS_PARAM _EXC_MASKS_PARAM \
           _EXC_INFO_PARAM) {\
     BID_PROLOG_TYPE_REF(type0, arg_name1)   \
     BID_SET_RND_MODE()         \
     OTHER_BID_PROLOG_REF()

#define       BID128_FUNCTION_ARGTYPE1_ARG128(fn_name, type1, arg_name1, arg_name2)\
      void fn_name (BID_UINT128 * pres, \
           type1 *pbid_##arg_name1,  BID_UINT128 *pbid_##arg_name2  \
           _RND_MODE_PARAM _EXC_FLAGS_PARAM _EXC_MASKS_PARAM \
           _EXC_INFO_PARAM) {\
     BID_PROLOG_TYPE_REF(type1, arg_name1)   \
     BID_PROLOG_REF(arg_name2)   \
     BID_SET_RND_MODE()         \
     OTHER_BID_PROLOG_REF()

#define       BID_TYPE0_FUNCTION_ARG128_ARGTYPE2(type0, fn_name, arg_name1, type2, arg_name2)\
      void fn_name (type0 *pres, \
           BID_UINT128 *pbid_##arg_name1,  type2 *pbid_##arg_name2  \
           _RND_MODE_PARAM _EXC_FLAGS_PARAM _EXC_MASKS_PARAM \
           _EXC_INFO_PARAM) {\
     BID_PROLOG_REF(arg_name1)   \
     BID_PROLOG_TYPE_REF(type2, arg_name2)   \
     BID_SET_RND_MODE()         \
     OTHER_BID_PROLOG_REF()

#define       BID_TYPE0_FUNCTION_ARGTYPE1_ARG128(type0, fn_name, type1, arg_name1, arg_name2)\
      void fn_name (type0 *pres, \
           type1 *pbid_##arg_name1,  BID_UINT128 *pbid_##arg_name2  \
           _RND_MODE_PARAM _EXC_FLAGS_PARAM _EXC_MASKS_PARAM \
           _EXC_INFO_PARAM) {\
     BID_PROLOG_TYPE_REF(type1, arg_name1)   \
     BID_PROLOG_REF(arg_name2)   \
     BID_SET_RND_MODE()         \
     OTHER_BID_PROLOG_REF()

#define       BID_TYPE0_FUNCTION_ARG128_ARG128(type0, fn_name, arg_name1, arg_name2)\
      void fn_name (type0 * pres, \
           BID_UINT128 *pbid_##arg_name1,  BID_UINT128 *pbid_##arg_name2  \
           _RND_MODE_PARAM _EXC_FLAGS_PARAM _EXC_MASKS_PARAM \
           _EXC_INFO_PARAM) {\
     BID_PROLOG_REF(arg_name1)   \
     BID_PROLOG_REF(arg_name2)   \
     BID_SET_RND_MODE()         \
     OTHER_BID_PROLOG_REF()

#define       BID_TYPE0_FUNCTION_ARG1(type0, fn_name, arg_name)\
      void fn_name (type0 * pres, \
           BID_UINT128 *  \
           pbid_##arg_name _RND_MODE_PARAM _EXC_FLAGS_PARAM _EXC_MASKS_PARAM \
           _EXC_INFO_PARAM) {\
     BID_PROLOG_REF(arg_name)   \
     BID_SET_RND_MODE()         \
     OTHER_BID_PROLOG_REF()

#define       BID128_FUNCTION_ARGTYPE1(fn_name, type1, arg_name)\
      void fn_name (BID_UINT128 * pres, \
           type1 *  \
           pbid_##arg_name _RND_MODE_PARAM _EXC_FLAGS_PARAM _EXC_MASKS_PARAM \
           _EXC_INFO_PARAM) {\
     BID_PROLOG_TYPE_REF(type1, arg_name)   \
     BID_SET_RND_MODE()         \
     OTHER_BID_PROLOG_REF()

#define       BID_TYPE0_FUNCTION_ARGTYPE1(type0, fn_name, type1, arg_name)\
      void fn_name (type0 * pres, \
           type1 *  \
           pbid_##arg_name _RND_MODE_PARAM _EXC_FLAGS_PARAM _EXC_MASKS_PARAM \
           _EXC_INFO_PARAM) {\
     BID_PROLOG_TYPE_REF(type1, arg_name)   \
     BID_SET_RND_MODE()         \
     OTHER_BID_PROLOG_REF()

#define       BID_RESTYPE0_FUNCTION_ARGTYPE1   BID_TYPE0_FUNCTION_ARGTYPE1

#define       BID_TYPE0_FUNCTION_ARGTYPE1_NORND(type0, fn_name, type1, arg_name)\
      void fn_name (type0 * pres, \
           type1 *  \
           pbid_##arg_name _EXC_FLAGS_PARAM _EXC_MASKS_PARAM \
           _EXC_INFO_PARAM) {\
     BID_PROLOG_TYPE_REF(type1, arg_name)   \
     OTHER_BID_PROLOG_REF()

#define       BID_TYPE0_FUNCTION_ARGTYPE1_NORND_DFP BID_TYPE0_FUNCTION_ARGTYPE1_NORND

#define       BID_TYPE0_FUNCTION_ARGTYPE1_NORND_NOFLAGS(type0, fn_name, type1, arg_name)\
      void fn_name (type0 * pres, \
           type1 *  \
           pbid_##arg_name _EXC_FLAGS_PARAM _EXC_MASKS_PARAM \
           _EXC_INFO_PARAM) {\
     BID_PROLOG_TYPE_REF(type1, arg_name)

#define       BID_TYPE0_FUNCTION_ARGTYPE1_ARGTYPE2_NORND(type0, fn_name, type1, arg_name1, type2, arg_name2)\
      void fn_name (type0 * pres, \
           type1 *  \
           pbid_##arg_name1, type2 * pbid_##arg_name2 _EXC_FLAGS_PARAM _EXC_MASKS_PARAM \
           _EXC_INFO_PARAM) {\
     BID_PROLOG_TYPE_REF(type1, arg_name1)   \
     BID_PROLOG_TYPE_REF(type2, arg_name2)   \
     OTHER_BID_PROLOG_REF()
//////////////////////////////////////////
/////////////////////////////////////////
////////////////////////////////////////

#else

//////////////////////////////////////////
/////////////////////////////////////////
////////////////////////////////////////

// BID args and result
#define       BID128_FUNCTION_ARG1(fn_name, arg_name)\
	 DFP_WRAPFN_DFP(128, fn_name, 128);              \
	                                                 \
DECLSPEC_OPT      BID_UINT128                                     \
     fn_name (BID_UINT128 bid_##arg_name _RND_MODE_PARAM _EXC_FLAGS_PARAM  \
           _EXC_MASKS_PARAM _EXC_INFO_PARAM) { \
     BID_PROLOG_VAL(arg_name)                      \
     OTHER_BID_PROLOG_VAL()

// BID args and result
#define       BID128_FUNCTION_ARG1_NORND(fn_name, arg_name)\
	 DFP_WRAPFN_DFP(128, fn_name, 128);              \
DECLSPEC_OPT      BID_UINT128                                     \
     fn_name (BID_UINT128 bid_##arg_name _EXC_FLAGS_PARAM  \
           _EXC_MASKS_PARAM _EXC_INFO_PARAM) { \
     BID_PROLOG_VAL(arg_name)                      \
     OTHER_BID_PROLOG_VAL()

// result is not BID type
#define       BID128_FUNCTION_ARG1_NORND_CUSTOMRESTYPE(restype, fn_name, arg_name)\
	 RES_WRAPFN_DFP(restype, fn_name, 128);              \
DECLSPEC_OPT      restype                                     \
     fn_name (BID_UINT128 bid_##arg_name _EXC_FLAGS_PARAM  \
           _EXC_MASKS_PARAM _EXC_INFO_PARAM) { \
     BID_PROLOG_VAL(arg_name)                      \
     OTHER_BID_PROLOG_VAL()

// BID args and result
#define       BID128_FUNCTION_ARG2(fn_name, arg_name1, arg_name2)\
	 DFP_WRAPFN_DFP_DFP(128, fn_name, 128, 128);                 \
	                                                             \
DECLSPEC_OPT      BID_UINT128                                     \
     fn_name (BID_UINT128 bid_##arg_name1,      \
            BID_UINT128 bid_##arg_name2 _RND_MODE_PARAM _EXC_FLAGS_PARAM  \
           _EXC_MASKS_PARAM _EXC_INFO_PARAM) {  \
     BID_PROLOG_VAL(arg_name1)                      \
     BID_PROLOG_VAL(arg_name2)                      \
     OTHER_BID_PROLOG_VAL()

// fmod, rem
#define       BID128_FUNCTION_ARG2_NORND(fn_name, arg_name1, arg_name2)\
	 DFP_WRAPFN_DFP_DFP(128, fn_name, 128, 128);                 \
	                                                             \
DECLSPEC_OPT      BID_UINT128                                     \
     fn_name (BID_UINT128 bid_##arg_name1,      \
            BID_UINT128 bid_##arg_name2 _EXC_FLAGS_PARAM  \
           _EXC_MASKS_PARAM _EXC_INFO_PARAM) {  \
     BID_PROLOG_VAL(arg_name1)                      \
     BID_PROLOG_VAL(arg_name2)                      \
     OTHER_BID_PROLOG_VAL()

// compares
#define       BID128_FUNCTION_ARG2_NORND_CUSTOMRESTYPE(restype, fn_name, arg_name1, arg_name2)\
	 RES_WRAPFN_DFP_DFP(restype, fn_name, 128, 128);                 \
DECLSPEC_OPT      restype                                    \
     fn_name (BID_UINT128 bid_##arg_name1,      \
            BID_UINT128 bid_##arg_name2 _EXC_FLAGS_PARAM  \
           _EXC_MASKS_PARAM _EXC_INFO_PARAM) {  \
     BID_PROLOG_VAL(arg_name1)                      \
     BID_PROLOG_VAL(arg_name2)                      \
     OTHER_BID_PROLOG_VAL()

// not currently used
#define       BID128_FUNCTION_ARG2P_NORND_CUSTOMRESTYPE(restype, fn_name, arg_name1, res_name2)\
	 RES_WRAPFN_DFP_DFP(restype, fn_name, 128, 128);                 \
DECLSPEC_OPT      restype                                    \
     fn_name (BID_UINT128 bid_##arg_name1,      \
            BID_UINT128* res_name2 _EXC_FLAGS_PARAM  \
           _EXC_MASKS_PARAM _EXC_INFO_PARAM) {  \
     BID_PROLOG_VAL(arg_name1)                      \
     OTHER_BID_PROLOG_VAL()

// not currently used
#define       BID128_FUNCTION_ARG3P_NORND_CUSTOMRESTYPE(restype, fn_name, arg_name1, arg_name2, res_name3)\
	 RES_WRAPFN_DFP_DFP_DFP(restype, fn_name, 128, 128, 128);                 \
DECLSPEC_OPT      restype                                    \
     fn_name (BID_UINT128 bid_##arg_name1,      \
            BID_UINT128 bid_##arg_name2, BID_UINT128* res_name3 _EXC_FLAGS_PARAM  \
           _EXC_MASKS_PARAM _EXC_INFO_PARAM) {  \
     BID_PROLOG_VAL(arg_name1)                      \
     BID_PROLOG_VAL(arg_name2)                      \
     OTHER_BID_PROLOG_VAL()

// BID args and result
#define       BID128_FUNCTION_ARG128_ARGTYPE2(fn_name, arg_name1, type2, arg_name2)\
	 DFP_WRAPFN_DFP_DFP(128, fn_name, 128, bidsize(type2));                 \
DECLSPEC_OPT      BID_UINT128                                     \
     fn_name (BID_UINT128 bid_##arg_name1,      \
            type2 bid_##arg_name2 _RND_MODE_PARAM _EXC_FLAGS_PARAM  \
           _EXC_MASKS_PARAM _EXC_INFO_PARAM) {  \
     BID_PROLOG_VAL(arg_name1)                      \
     BID_PROLOG_TYPE_VAL(type2, arg_name2)          \
     OTHER_BID_PROLOG_VAL()

// scalb, ldexp
#define       BID128_FUNCTION_ARG128_CUSTOMARGTYPE2(fn_name, arg_name1, type2, arg_name2)\
	 DFP_WRAPFN_DFP_OTHERTYPE(128, fn_name, 128, type2);                 \
DECLSPEC_OPT      BID_UINT128                                     \
     fn_name (BID_UINT128 bid_##arg_name1,      \
            type2 bid_##arg_name2 _RND_MODE_PARAM _EXC_FLAGS_PARAM  \
           _EXC_MASKS_PARAM _EXC_INFO_PARAM) {  \
     BID_PROLOG_VAL(arg_name1)                      \
     BID_PROLOG_TYPE_VAL(type2, arg_name2)          \
     OTHER_BID_PROLOG_VAL()

// frexp
#define       BID128_FUNCTION_ARG128_CUSTOMARGTYPE2_PLAIN(fn_name, arg_name1, type2, arg_name2)\
	 DFP_WRAPFN_DFP_OTHERTYPE(128, fn_name, 128, type2);                 \
DECLSPEC_OPT      BID_UINT128                                     \
     fn_name (BID_UINT128 bid_##arg_name1,      \
            type2 arg_name2   \
           ) {  \
     BID_PROLOG_VAL(arg_name1)                      \
     OTHER_BID_PROLOG_VAL()

// BID args and result
#define       BID_TYPE0_FUNCTION_ARGTYPE1_ARGTYPE2(type0, fn_name, type1, arg_name1, type2, arg_name2)\
	 DFP_WRAPFN_DFP_DFP(bidsize(type0), fn_name, bidsize(type1), bidsize(type2));                 \
DECLSPEC_OPT      type0                                     \
     fn_name (type1 bid_##arg_name1,      \
            type2 bid_##arg_name2 _RND_MODE_PARAM _EXC_FLAGS_PARAM  \
           _EXC_MASKS_PARAM _EXC_INFO_PARAM) {  \
     BID_PROLOG_TYPE_VAL(type1, arg_name1)                      \
     BID_PROLOG_TYPE_VAL(type2, arg_name2)          \
     OTHER_BID_PROLOG_VAL()

// BID arg1 and result
#define       BID_TYPE0_FUNCTION_ARGTYPE1_OTHER_ARGTYPE2(type0, fn_name, type1, arg_name1, type2, arg_name2)\
	 DFP_WRAPFN_DFP_OTHERTYPE(bidsize(type0), fn_name, bidsize(type1), type2);                 \
DECLSPEC_OPT      type0                                     \
     fn_name (type1 bid_##arg_name1,      \
            type2 bid_##arg_name2 _RND_MODE_PARAM _EXC_FLAGS_PARAM  \
           _EXC_MASKS_PARAM _EXC_INFO_PARAM) {  \
     BID_PROLOG_TYPE_VAL(type1, arg_name1)                      \
     BID_PROLOG_TYPE_VAL(type2, arg_name2)          \
     OTHER_BID_PROLOG_VAL()

// BID args and result
#define       BID_TYPE0_FUNCTION_ARGTYPE1_ARGTYPE2_ARGTYPE3(type0, fn_name, type1, arg_name1, type2, arg_name2, type3, arg_name3)\
	 DFP_WRAPFN_DFP_DFP_DFP(bidsize(type0), fn_name, bidsize(type1), bidsize(type2), bidsize(type3));                 \
DECLSPEC_OPT      type0                                     \
     fn_name (type1 bid_##arg_name1,      \
            type2 bid_##arg_name2, type3 bid_##arg_name3 _RND_MODE_PARAM _EXC_FLAGS_PARAM  \
           _EXC_MASKS_PARAM _EXC_INFO_PARAM) {  \
     BID_PROLOG_TYPE_VAL(type1, arg_name1)                      \
     BID_PROLOG_TYPE_VAL(type2, arg_name2)          \
     BID_PROLOG_TYPE_VAL(type3, arg_name3)          \
     OTHER_BID_PROLOG_VAL()

// BID args and result
#define       BID_TYPE_FUNCTION_ARG2(type0, fn_name, arg_name1, arg_name2)\
	 DFP_WRAPFN_DFP_DFP(bidsize(type0), fn_name, bidsize(type0), bidsize(type0));                 \
DECLSPEC_OPT      type0                                     \
     fn_name (type0 bid_##arg_name1,      \
            type0 bid_##arg_name2 _RND_MODE_PARAM _EXC_FLAGS_PARAM  \
           _EXC_MASKS_PARAM _EXC_INFO_PARAM) {  \
     BID_PROLOG_TYPE_VAL(type0, arg_name1)                      \
     BID_PROLOG_TYPE_VAL(type0, arg_name2)          \
     OTHER_BID_PROLOG_VAL()

// BID args, result a different type (e.g. for compares)
#define       BID_TYPE_FUNCTION_ARG2_CUSTOMRESULT_NORND(typeres, fn_name, type0, arg_name1, arg_name2)\
	 RES_WRAPFN_DFP_DFP(typeres, fn_name, bidsize(type0), bidsize(type0));                 \
DECLSPEC_OPT      typeres                                     \
     fn_name (type0 bid_##arg_name1,      \
            type0 bid_##arg_name2 _EXC_FLAGS_PARAM  \
           _EXC_MASKS_PARAM _EXC_INFO_PARAM) {  \
     BID_PROLOG_TYPE_VAL(type0, arg_name1)                      \
     BID_PROLOG_TYPE_VAL(type0, arg_name2)          \
     OTHER_BID_PROLOG_VAL()

// BID args and result
#define       BID_TYPE_FUNCTION_ARG1(type0, fn_name, arg_name1)\
	 DFP_WRAPFN_DFP(bidsize(type0), fn_name, bidsize(type0));                 \
DECLSPEC_OPT      type0                                     \
     fn_name (type0 bid_##arg_name1      \
             _RND_MODE_PARAM _EXC_FLAGS_PARAM  \
           _EXC_MASKS_PARAM _EXC_INFO_PARAM) {  \
     BID_PROLOG_TYPE_VAL(type0, arg_name1)                      \
     OTHER_BID_PROLOG_VAL()

// BID args and result
#define       BID128_FUNCTION_ARGTYPE1_ARG128(fn_name, type1, arg_name1, arg_name2)\
	 DFP_WRAPFN_DFP_DFP(128, fn_name, bidsize(type1), 128);                 \
DECLSPEC_OPT      BID_UINT128                                     \
     fn_name (type1 bid_##arg_name1,      \
            BID_UINT128 bid_##arg_name2 _RND_MODE_PARAM _EXC_FLAGS_PARAM  \
           _EXC_MASKS_PARAM _EXC_INFO_PARAM) {  \
     BID_PROLOG_TYPE_VAL(type1, arg_name1)          \
     BID_PROLOG_VAL(arg_name2)                      \
     OTHER_BID_PROLOG_VAL()

// BID args and result
#define       BID_TYPE0_FUNCTION_ARG128_ARGTYPE2(type0, fn_name, arg_name1, type2, arg_name2)\
	 DFP_WRAPFN_DFP_DFP(bidsize(type0), fn_name, 128, bidsize(type2));                 \
DECLSPEC_OPT      type0                                     \
     fn_name (BID_UINT128 bid_##arg_name1,      \
            type2 bid_##arg_name2 _RND_MODE_PARAM _EXC_FLAGS_PARAM  \
           _EXC_MASKS_PARAM _EXC_INFO_PARAM) {  \
     BID_PROLOG_VAL(arg_name1)                      \
     BID_PROLOG_TYPE_VAL(type2, arg_name2)          \
     OTHER_BID_PROLOG_VAL()

// BID args and result
#define       BID_TYPE0_FUNCTION_ARGTYPE1_ARG128(type0, fn_name, type1, arg_name1, arg_name2)\
	 DFP_WRAPFN_DFP_DFP(bidsize(type0), fn_name, bidsize(type1), 128);                 \
DECLSPEC_OPT      type0                                     \
     fn_name (type1 bid_##arg_name1,      \
            BID_UINT128 bid_##arg_name2 _RND_MODE_PARAM _EXC_FLAGS_PARAM  \
           _EXC_MASKS_PARAM _EXC_INFO_PARAM) {  \
     BID_PROLOG_TYPE_VAL(type1, arg_name1)                      \
     BID_PROLOG_VAL(arg_name2)          \
     OTHER_BID_PROLOG_VAL()

// BID args and result
#define       BID_TYPE0_FUNCTION_ARG128_ARG128(type0, fn_name, arg_name1, arg_name2)\
	 DFP_WRAPFN_DFP_DFP(bidsize(type0), fn_name, 128, 128);                 \
DECLSPEC_OPT      type0                                     \
     fn_name (BID_UINT128 bid_##arg_name1,      \
            BID_UINT128 bid_##arg_name2 _RND_MODE_PARAM _EXC_FLAGS_PARAM  \
           _EXC_MASKS_PARAM _EXC_INFO_PARAM) {  \
     BID_PROLOG_VAL(arg_name1)                      \
     BID_PROLOG_VAL(arg_name2)                      \
     OTHER_BID_PROLOG_VAL()

// BID args and result
#define       BID_TYPE0_FUNCTION_ARG1(type0, fn_name, arg_name)\
	 DFP_WRAPFN_DFP(bidsize(type0), fn_name, 128);                 \
DECLSPEC_OPT      type0                                     \
     fn_name (BID_UINT128 bid_##arg_name _RND_MODE_PARAM _EXC_FLAGS_PARAM  \
           _EXC_MASKS_PARAM _EXC_INFO_PARAM) { \
     BID_PROLOG_VAL(arg_name)                      \
     OTHER_BID_PROLOG_VAL()

// BID args and result
#define       BID128_FUNCTION_ARGTYPE1(fn_name, type1, arg_name)\
	 DFP_WRAPFN_DFP(128, fn_name, bidsize(type1));                 \
DECLSPEC_OPT      BID_UINT128                                     \
     fn_name (type1 bid_##arg_name _RND_MODE_PARAM _EXC_FLAGS_PARAM  \
           _EXC_MASKS_PARAM _EXC_INFO_PARAM) { \
     BID_PROLOG_TYPE_VAL(type1, arg_name)                      \
     OTHER_BID_PROLOG_VAL()

// BID args and result
#define       BID_TYPE0_FUNCTION_ARGTYPE1(type0, fn_name, type1, arg_name)\
	 DFP_WRAPFN_DFP(bidsize(type0), fn_name, bidsize(type1))                 \
DECLSPEC_OPT      type0                                     \
     fn_name (type1 bid_##arg_name _RND_MODE_PARAM _EXC_FLAGS_PARAM  \
           _EXC_MASKS_PARAM _EXC_INFO_PARAM) { \
     BID_PROLOG_TYPE_VAL(type1, arg_name)                      \
     OTHER_BID_PROLOG_VAL()

// BID args and result
#define       BID_TYPE0_FUNCTION_ARGTYPE1_NORND_DFP(type0, fn_name, type1, arg_name)\
	 DFP_WRAPFN_DFP(bidsize(type0), fn_name, bidsize(type1))                 \
DECLSPEC_OPT      type0                                     \
     fn_name (type1 bid_##arg_name _EXC_FLAGS_PARAM  \
           _EXC_MASKS_PARAM _EXC_INFO_PARAM) { \
     BID_PROLOG_TYPE_VAL(type1, arg_name)                      \
     OTHER_BID_PROLOG_VAL()

// BID args, different type result
#define       BID_RESTYPE0_FUNCTION_ARGTYPE1(type0, fn_name, type1, arg_name)\
	 RES_WRAPFN_DFP(type0, fn_name, bidsize(type1))                 \
DECLSPEC_OPT      type0                                     \
     fn_name (type1 bid_##arg_name _RND_MODE_PARAM _EXC_FLAGS_PARAM  \
           _EXC_MASKS_PARAM _EXC_INFO_PARAM) { \
     BID_PROLOG_TYPE_VAL(type1, arg_name)                      \
     OTHER_BID_PROLOG_VAL()

// BID to int/uint functions
#define       BID_TYPE0_FUNCTION_ARGTYPE1_NORND(type0, fn_name, type1, arg_name)\
	 RES_WRAPFN_DFP(type0, fn_name, bidsize(type1));                 \
DECLSPEC_OPT      type0                                     \
     fn_name (type1 bid_##arg_name _EXC_FLAGS_PARAM  \
           _EXC_MASKS_PARAM _EXC_INFO_PARAM) { \
     BID_PROLOG_TYPE_VAL(type1, arg_name)                      \
     OTHER_BID_PROLOG_VAL()

// used for BID-to-BID conversions
#define       BID_TYPE0_FUNCTION_ARGTYPE1_NORND_NOFLAGS(type0, fn_name, type1, arg_name)\
	 DFP_WRAPFN_DFP(bidsize(type0), fn_name, bidsize(type1));                 \
DECLSPEC_OPT      type0                                     \
     fn_name (type1 bid_##arg_name _EXC_FLAGS_PARAM  \
           _EXC_MASKS_PARAM _EXC_INFO_PARAM) { \
     BID_PROLOG_TYPE_VAL(type1, arg_name)

// fmod, rem
#define       BID_TYPE0_FUNCTION_ARGTYPE1_ARGTYPE2_NORND(type0, fn_name, type1, arg_name1, type2, arg_name2)\
	 DFP_WRAPFN_DFP_DFP(bidsize(type0), fn_name, bidsize(type1), bidsize(type2));                 \
DECLSPEC_OPT      type0                                     \
     fn_name (type1 bid_##arg_name1, type2 bid_##arg_name2 _EXC_FLAGS_PARAM  \
           _EXC_MASKS_PARAM _EXC_INFO_PARAM) { \
     BID_PROLOG_TYPE_VAL(type1, arg_name1)                      \
     BID_PROLOG_TYPE_VAL(type2, arg_name2)                      \
     OTHER_BID_PROLOG_VAL()
#endif



#define   BID_TO_SMALL_BID_UINT_CVT_FUNCTION(type0, fn_name, type1, arg_name, cvt_fn_name, type2, size_mask, invalid_res)\
    BID_TYPE0_FUNCTION_ARGTYPE1_NORND(type0, fn_name, type1, arg_name)\
        type2 res;                                                    \
        _IDEC_flags saved_fpsc=*pfpsf;                                \
    BIDECIMAL_CALL1_NORND(cvt_fn_name, res, arg_name);            \
        if(res & size_mask) {                                         \
      *pfpsf = saved_fpsc | BID_INVALID_EXCEPTION;                    \
          res = invalid_res; }                                        \
    BID_RETURN_VAL((type0)res);                                   \
                   }

#define   BID_TO_SMALL_INT_CVT_FUNCTION(type0, fn_name, type1, arg_name, cvt_fn_name, type2, size_mask, invalid_res)\
    BID_TYPE0_FUNCTION_ARGTYPE1_NORND(type0, fn_name, type1, arg_name)\
        type2 res, sgn_mask;                                          \
        _IDEC_flags saved_fpsc=*pfpsf;                                \
    BIDECIMAL_CALL1_NORND(cvt_fn_name, res, arg_name);            \
        sgn_mask = res & size_mask;                                   \
        if(sgn_mask && (sgn_mask != (type2)size_mask)) {                     \
      *pfpsf = saved_fpsc | BID_INVALID_EXCEPTION;                    \
          res = invalid_res; }                                        \
    BID_RETURN_VAL((type0)res);                                   \
                   }
#endif

