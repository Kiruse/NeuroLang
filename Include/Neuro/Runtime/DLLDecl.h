/*******************************************************************************
 * Utility header to assist in the definition of DLL import / export macros.
 * ---
 * Copyright (c) Kiruse. See license in LICENSE.txt, or online at http://neuro.kirusifix.com/license.
 */

#ifndef NEURO_DLLDECL_H
#define NEURO_DLLDECL_H

#ifdef BUILD_NEURO_SHARED
# ifdef _WIN32
#  ifdef BUILD_NEURO_API
#   define NEURO_API __declspec(dllexport)
#  else
#   define NEURO_API __declspec(dllimport)
#  endif
# else
#  define NEURO_API
# endif
#else
# define NEURO_API
#endif

#endif /* NEURO_DLLDECL_H */
