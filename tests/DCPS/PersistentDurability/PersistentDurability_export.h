
// -*- C++ -*-
// $Id$
// Definition for Win32 Export directives.
// This file is generated automatically by generate_export_file.pl PersistentDurability
// ------------------------------
#ifndef PERSISTENTDURABILITY_EXPORT_H
#define PERSISTENTDURABILITY_EXPORT_H

#include "ace/config-all.h"

#if defined (ACE_AS_STATIC_LIBS) && !defined (PERSISTENTDURABILITY_HAS_DLL)
#  define PERSISTENTDURABILITY_HAS_DLL 0
#endif /* ACE_AS_STATIC_LIBS && PERSISTENTDURABILITY_HAS_DLL */

#if !defined (PERSISTENTDURABILITY_HAS_DLL)
#  define PERSISTENTDURABILITY_HAS_DLL 1
#endif /* ! PERSISTENTDURABILITY_HAS_DLL */

#if defined (PERSISTENTDURABILITY_HAS_DLL) && (PERSISTENTDURABILITY_HAS_DLL == 1)
#  if defined (PERSISTENTDURABILITY_BUILD_DLL)
#    define PersistentDurability_Export ACE_Proper_Export_Flag
#    define PERSISTENTDURABILITY_SINGLETON_DECLARATION(T) ACE_EXPORT_SINGLETON_DECLARATION (T)
#    define PERSISTENTDURABILITY_SINGLETON_DECLARE(SINGLETON_TYPE, CLASS, LOCK) ACE_EXPORT_SINGLETON_DECLARE(SINGLETON_TYPE, CLASS, LOCK)
#  else /* PERSISTENTDURABILITY_BUILD_DLL */
#    define PersistentDurability_Export ACE_Proper_Import_Flag
#    define PERSISTENTDURABILITY_SINGLETON_DECLARATION(T) ACE_IMPORT_SINGLETON_DECLARATION (T)
#    define PERSISTENTDURABILITY_SINGLETON_DECLARE(SINGLETON_TYPE, CLASS, LOCK) ACE_IMPORT_SINGLETON_DECLARE(SINGLETON_TYPE, CLASS, LOCK)
#  endif /* PERSISTENTDURABILITY_BUILD_DLL */
#else /* PERSISTENTDURABILITY_HAS_DLL == 1 */
#  define PersistentDurability_Export
#  define PERSISTENTDURABILITY_SINGLETON_DECLARATION(T)
#  define PERSISTENTDURABILITY_SINGLETON_DECLARE(SINGLETON_TYPE, CLASS, LOCK)
#endif /* PERSISTENTDURABILITY_HAS_DLL == 1 */

// Set PERSISTENTDURABILITY_NTRACE = 0 to turn on library specific tracing even if
// tracing is turned off for ACE.
#if !defined (PERSISTENTDURABILITY_NTRACE)
#  if (ACE_NTRACE == 1)
#    define PERSISTENTDURABILITY_NTRACE 1
#  else /* (ACE_NTRACE == 1) */
#    define PERSISTENTDURABILITY_NTRACE 0
#  endif /* (ACE_NTRACE == 1) */
#endif /* !PERSISTENTDURABILITY_NTRACE */

#if (PERSISTENTDURABILITY_NTRACE == 1)
#  define PERSISTENTDURABILITY_TRACE(X)
#else /* (PERSISTENTDURABILITY_NTRACE == 1) */
#  if !defined (ACE_HAS_TRACE)
#    define ACE_HAS_TRACE
#  endif /* ACE_HAS_TRACE */
#  define PERSISTENTDURABILITY_TRACE(X) ACE_TRACE_IMPL(X)
#  include "ace/Trace.h"
#endif /* (PERSISTENTDURABILITY_NTRACE == 1) */

#endif /* PERSISTENTDURABILITY_EXPORT_H */

// End of auto generated file.
