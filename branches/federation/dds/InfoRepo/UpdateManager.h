// -*- C++ -*-
/**
 * @file      UpdateManager.h
 *
 * library   UpdateManager
 *
 * $Id$
 *
 * @author Ciju John <johnc@ociweb.com>
 */
#ifndef _UPDATE_MANAGER_
#define _UPDATE_MANAGER_

#include "UpdateDataTypes.h"
#include "Updater.h"

#include "dds/DdsDcpsInfrastructureC.h"
#include "dds/DdsDcpsInfoUtilsC.h"

#include "tao/CDR.h"

#include "ace/Service_Object.h"
#include "ace/Service_Config.h"

#include <set>

// forward declarations
class TAO_DDS_DCPSInfo_i;

namespace Update {

class UpdateManager : public ACE_Service_Object
{
 public:
  UpdateManager (void);

  virtual ~UpdateManager (void);

  /// Shared object initializer
  virtual int init (int argc, ACE_TCHAR *argv[]);

  /// Shared object finalizer
  virtual int fini (void);

  // mechanism for InfoRepo object to be registered.
  void add (TAO_DDS_DCPSInfo_i* info);
  void add (Updater* updater);

  // Mechanism to unregister Updaters/InfoRepo
  void remove ();
  void remove (const Updater* updater);

  /// Force a clean shutdown.
  // void shutdown (void);

  /// Upstream request for a fresh image
  /// Currently handled synchronously via 'pushImage'
  /// TBD: Replace with an asynchronous model.
  void requestImage (void);

  /// Downstream request to push image
  void pushImage (const DImage& image);

  // Propagate creation of entities.
  template< class UType>
  void create( const UType& info);

  // Propagate QoS updates.
  template< class QosType>
  void update( const IdType& id, const QosType& qos);

  // Propagate destruction of entities.
  void destroy( ItemType type, const IdType& id);

  // Downstream request to push persisted data
  void add (const DTopic& topic);
  void add (const DParticipant& participant);
  void add (const DActor& actor);

 private:
  typedef std::set <Updater*> Updaters;

  // required to break an include dependency loop
  //void add (Updater* updater, const DActor& actor);

  TAO_DDS_DCPSInfo_i* info_;
  Updaters updaters_;
};

} // End of namespace Update

#if defined (ACE_TEMPLATES_REQUIRE_SOURCE)
#include "UpdateManager_T.cpp"
#endif /* ACE_TEMPLATES_REQUIRE_SOURCE */

#if defined (ACE_TEMPLATES_REQUIRE_PRAGMA)
#pragma message ("UpdateManager_T.cpp template inst")
#pragma implementation ("UpdateManager_T.cpp")
#endif /* ACE_TEMPLATES_REQUIRE_PRAGMA */

typedef Update::UpdateManager UpdateManagerSvc;

ACE_STATIC_SVC_DECLARE (UpdateManagerSvc)

ACE_FACTORY_DECLARE (ACE_Local_Service, UpdateManagerSvc)

class UpdateManagerSvc_Loader
{
public:
  static int init (void);
};

#if defined(ACE_HAS_BROKEN_STATIC_CONSTRUCTORS)

typedef int (*UpdateManagerSvc_Loader) (void);

static UpdateManagerSvc_Loader ldr =
&UpdateManagerSvc_Loader::init;

#else

static int ldr =
UpdateManagerSvc_Loader::init ();

#endif /* ACE_HAS_BROKEN_STATIC_CONSTRUCTORS */

#endif // _UPDATE_MANAGER_
