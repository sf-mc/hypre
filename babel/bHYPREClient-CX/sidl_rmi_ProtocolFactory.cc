// 
// File:          sidl_rmi_ProtocolFactory.cc
// Symbol:        sidl.rmi.ProtocolFactory-v0.9.3
// Symbol Type:   class
// Babel Version: 0.10.12
// Release:       $Name$
// Revision:      @(#) $Id$
// Description:   Client-side glue code for sidl.rmi.ProtocolFactory
// 
// Copyright (c) 2000-2002, The Regents of the University of California.
// Produced at the Lawrence Livermore National Laboratory.
// Written by the Components Team <components@llnl.gov>
// All rights reserved.
// 
// This file is part of Babel. For more information, see
// http://www.llnl.gov/CASC/components/. Please read the COPYRIGHT file
// for Our Notice and the LICENSE file for the GNU Lesser General Public
// License.
// 
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU Lesser General Public License (as published by
// the Free Software Foundation) version 2.1 dated February 1999.
// 
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the IMPLIED WARRANTY OF
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the terms and
// conditions of the GNU Lesser General Public License for more details.
// 
// You should have recieved a copy of the GNU Lesser General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
// 
// WARNING: Automatically generated; changes will be lost
// 
// babel-version = 0.10.12
// xml-url       = /home/painter/babel-0.10.12/bin/.././share/repository/sidl.rmi.ProtocolFactory-v0.9.3.xml
// 

#ifndef included_sidl_rmi_ProtocolFactory_hh
#include "sidl_rmi_ProtocolFactory.hh"
#endif
#ifndef included_sidl_BaseInterface_hh
#include "sidl_BaseInterface.hh"
#endif
#ifndef included_sidl_BaseClass_hh
#include "sidl_BaseClass.hh"
#endif
#ifndef included_sidl_BaseException_hh
#include "sidl_BaseException.hh"
#endif
#include "sidl_String.h"
// 
// Includes for all method dependencies.
// 
#ifndef included_sidl_BaseInterface_hh
#include "sidl_BaseInterface.hh"
#endif
#ifndef included_sidl_ClassInfo_hh
#include "sidl_ClassInfo.hh"
#endif
#ifndef included_sidl_rmi_InstanceHandle_hh
#include "sidl_rmi_InstanceHandle.hh"
#endif
#ifndef included_sidl_rmi_NetworkException_hh
#include "sidl_rmi_NetworkException.hh"
#endif


//////////////////////////////////////////////////
// 
// User Defined Methods
// 


/**
 * Associate a particular prefix in the URL to a typeName <code>sidl.Loader</code> can find.
 * The actual type is expected to implement <code>sidl.rmi.InstanceHandle</code>
 * Return true iff the addition is successful.  (no collisions allowed)
 */
bool
ucxx::sidl::rmi::ProtocolFactory::addProtocol( /* in */const ::std::string& 
  prefix, /* in */const ::std::string& typeName )
throw ( ::ucxx::sidl::rmi::NetworkException)

{
  bool _result;
  sidl_bool _local_result;
  sidl_BaseInterface__object * _exception = 0;
  /*pack args to dispatch to ior*/
  _local_result = ( _get_sepv()->f_addProtocol)( /* in */ prefix.c_str(),
    /* in */ typeName.c_str(), &_exception );
  /*dispatch to ior*/
  if (_exception != 0 ) {
    void * _p = 0;
    if ( (_p=(*(_exception->d_epv->f__cast))(_exception->d_object,
      "sidl.rmi.NetworkException")) != 0 ) {
      struct sidl_rmi_NetworkException__object * _realtype = reinterpret_cast< 
        struct sidl_rmi_NetworkException__object*>(_p);
      // Note: alternate constructor does not increment refcount.
      throw ::ucxx::sidl::rmi::NetworkException( _realtype, false );
    }
  }
  _result = _local_result;
  /*unpack results and cleanup*/
  return _result;
}



/**
 * Return the typeName associated with a particular prefix.
 * Return empty string if the prefix
 */
::std::string
ucxx::sidl::rmi::ProtocolFactory::getProtocol( /* in */const ::std::string& 
  prefix )
throw ( ::ucxx::sidl::rmi::NetworkException)

{
  ::std::string _result;
  char * _local_result;
  sidl_BaseInterface__object * _exception = 0;
  /*pack args to dispatch to ior*/
  _local_result = ( _get_sepv()->f_getProtocol)( /* in */ prefix.c_str(),
    &_exception );
  /*dispatch to ior*/
  if (_exception != 0 ) {
    void * _p = 0;
    if ( (_p=(*(_exception->d_epv->f__cast))(_exception->d_object,
      "sidl.rmi.NetworkException")) != 0 ) {
      struct sidl_rmi_NetworkException__object * _realtype = reinterpret_cast< 
        struct sidl_rmi_NetworkException__object*>(_p);
      // Note: alternate constructor does not increment refcount.
      throw ::ucxx::sidl::rmi::NetworkException( _realtype, false );
    }
  }
  if (_local_result) {
    _result = _local_result;
    free( _local_result );
  }
  /*unpack results and cleanup*/
  return _result;
}



/**
 * Remove a protocol from the active list.
 */
bool
ucxx::sidl::rmi::ProtocolFactory::deleteProtocol( /* in */const ::std::string& 
  prefix )
throw ( ::ucxx::sidl::rmi::NetworkException)

{
  bool _result;
  sidl_bool _local_result;
  sidl_BaseInterface__object * _exception = 0;
  /*pack args to dispatch to ior*/
  _local_result = ( _get_sepv()->f_deleteProtocol)( /* in */ prefix.c_str(),
    &_exception );
  /*dispatch to ior*/
  if (_exception != 0 ) {
    void * _p = 0;
    if ( (_p=(*(_exception->d_epv->f__cast))(_exception->d_object,
      "sidl.rmi.NetworkException")) != 0 ) {
      struct sidl_rmi_NetworkException__object * _realtype = reinterpret_cast< 
        struct sidl_rmi_NetworkException__object*>(_p);
      // Note: alternate constructor does not increment refcount.
      throw ::ucxx::sidl::rmi::NetworkException( _realtype, false );
    }
  }
  _result = _local_result;
  /*unpack results and cleanup*/
  return _result;
}



/**
 * Create a new remote object and a return an instance handle for that object. 
 * The server and port number are in the url.  Return nil 
 * if protocol unknown or InstanceHandle.init() failed.
 */
::ucxx::sidl::rmi::InstanceHandle
ucxx::sidl::rmi::ProtocolFactory::createInstance( /* in */const ::std::string& 
  url, /* in */const ::std::string& typeName )
throw ( ::ucxx::sidl::rmi::NetworkException)

{
  ::ucxx::sidl::rmi::InstanceHandle _result;
  sidl_BaseInterface__object * _exception = 0;
  /*pack args to dispatch to ior*/
  _result = ::ucxx::sidl::rmi::InstanceHandle( ( 
    _get_sepv()->f_createInstance)( /* in */ url.c_str(),
    /* in */ typeName.c_str(), &_exception ), false);
  /*dispatch to ior*/
  if (_exception != 0 ) {
    void * _p = 0;
    if ( (_p=(*(_exception->d_epv->f__cast))(_exception->d_object,
      "sidl.rmi.NetworkException")) != 0 ) {
      struct sidl_rmi_NetworkException__object * _realtype = reinterpret_cast< 
        struct sidl_rmi_NetworkException__object*>(_p);
      // Note: alternate constructor does not increment refcount.
      throw ::ucxx::sidl::rmi::NetworkException( _realtype, false );
    }
  }
  /*unpack results and cleanup*/
  return _result;
}



/**
 * Create an new connection linked to an already existing object on a remote 
 * server.  The server and port number are in the url, the objectID is the unique ID
 * of the remote object in the remote instance registry. 
 * Return nil if protocol unknown or InstanceHandle.init() failed.
 */
::ucxx::sidl::rmi::InstanceHandle
ucxx::sidl::rmi::ProtocolFactory::connectInstance( /* in */const ::std::string& 
  url )
throw ( ::ucxx::sidl::rmi::NetworkException)

{
  ::ucxx::sidl::rmi::InstanceHandle _result;
  sidl_BaseInterface__object * _exception = 0;
  /*pack args to dispatch to ior*/
  _result = ::ucxx::sidl::rmi::InstanceHandle( ( 
    _get_sepv()->f_connectInstance)( /* in */ url.c_str(), &_exception ),
    false);
  /*dispatch to ior*/
  if (_exception != 0 ) {
    void * _p = 0;
    if ( (_p=(*(_exception->d_epv->f__cast))(_exception->d_object,
      "sidl.rmi.NetworkException")) != 0 ) {
      struct sidl_rmi_NetworkException__object * _realtype = reinterpret_cast< 
        struct sidl_rmi_NetworkException__object*>(_p);
      // Note: alternate constructor does not increment refcount.
      throw ::ucxx::sidl::rmi::NetworkException( _realtype, false );
    }
  }
  /*unpack results and cleanup*/
  return _result;
}



/**
 * Return true if and only if <code>obj</code> refers to the same
 * object as this object.
 */
bool
ucxx::sidl::rmi::ProtocolFactory::isSame( /* in */::ucxx::sidl::BaseInterface 
  iobj )
throw ()

{
  bool _result;
  ior_t* loc_self = _get_ior();
  sidl_bool _local_result;
  struct sidl_BaseInterface__object* _local_iobj = reinterpret_cast< struct 
    sidl_BaseInterface__object* > ( iobj._get_ior() ? ((*((reinterpret_cast< 
    struct sidl_BaseInterface__object * > 
    (iobj._get_ior()))->d_epv->f__cast))((reinterpret_cast< struct 
    sidl_BaseInterface__object * > (iobj._get_ior()))->d_object,
    "sidl.BaseInterface")) : 0);
  /*pack args to dispatch to ior*/
  _local_result = (*(loc_self->d_epv->f_isSame))(loc_self,
    /* in */ _local_iobj );
  /*dispatch to ior*/
  _result = _local_result;
  /*unpack results and cleanup*/
  return _result;
}



/**
 * Check whether the object can support the specified interface or
 * class.  If the <code>sidl</code> type name in <code>name</code>
 * is supported, then a reference to that object is returned with the
 * reference count incremented.  The callee will be responsible for
 * calling <code>deleteRef</code> on the returned object.  If
 * the specified type is not supported, then a null reference is
 * returned.
 */
::ucxx::sidl::BaseInterface
ucxx::sidl::rmi::ProtocolFactory::queryInt( /* in */const ::std::string& name )
throw ()

{
  ::ucxx::sidl::BaseInterface _result;
  ior_t* loc_self = _get_ior();
  /*pack args to dispatch to ior*/
  _result = ::ucxx::sidl::BaseInterface( 
    (*(loc_self->d_epv->f_queryInt))(loc_self, /* in */ name.c_str() ), false);
  /*dispatch to ior*/
  /*unpack results and cleanup*/
  return _result;
}



/**
 * Return whether this object is an instance of the specified type.
 * The string name must be the <code>sidl</code> type name.  This
 * routine will return <code>true</code> if and only if a cast to
 * the string type name would succeed.
 */
bool
ucxx::sidl::rmi::ProtocolFactory::isType( /* in */const ::std::string& name )
throw ()

{
  bool _result;
  ior_t* loc_self = _get_ior();
  sidl_bool _local_result;
  /*pack args to dispatch to ior*/
  _local_result = (*(loc_self->d_epv->f_isType))(loc_self,
    /* in */ name.c_str() );
  /*dispatch to ior*/
  _result = _local_result;
  /*unpack results and cleanup*/
  return _result;
}



/**
 * Return the meta-data about the class implementing this interface.
 */
::ucxx::sidl::ClassInfo
ucxx::sidl::rmi::ProtocolFactory::getClassInfo(  )
throw ()

{
  ::ucxx::sidl::ClassInfo _result;
  ior_t* loc_self = _get_ior();
  /*pack args to dispatch to ior*/
  _result = ::ucxx::sidl::ClassInfo( 
    (*(loc_self->d_epv->f_getClassInfo))(loc_self ), false);
  /*dispatch to ior*/
  /*unpack results and cleanup*/
  return _result;
}



//////////////////////////////////////////////////
// 
// End User Defined Methods
// (everything else in this file is specific to
//  Babel's C++ bindings)
// 

// static constructor
::ucxx::sidl::rmi::ProtocolFactory
ucxx::sidl::rmi::ProtocolFactory::_create() {
  ::ucxx::sidl::rmi::ProtocolFactory self( (*_get_ext()->createObject)(),
    false );
  return self;
}

// copy constructor
ucxx::sidl::rmi::ProtocolFactory::ProtocolFactory ( const 
  ::ucxx::sidl::rmi::ProtocolFactory& original ) {
  d_self = original._cast("sidl.rmi.ProtocolFactory");
  d_weak_reference = original.d_weak_reference;
  if (d_self != 0 ) {
    addRef();
  }
}

// assignment operator
::ucxx::sidl::rmi::ProtocolFactory&
ucxx::sidl::rmi::ProtocolFactory::operator=( const 
  ::ucxx::sidl::rmi::ProtocolFactory& rhs ) {
  if ( d_self != rhs.d_self ) {
    if ( d_self != 0 ) {
      deleteRef();
    }
    d_self = rhs._cast("sidl.rmi.ProtocolFactory");
    d_weak_reference = rhs.d_weak_reference;
    if ( d_self != 0 ) {
      addRef();
    }
  }
  return *this;
}

// conversion from ior to C++ class
ucxx::sidl::rmi::ProtocolFactory::ProtocolFactory ( 
  ::ucxx::sidl::rmi::ProtocolFactory::ior_t* ior ) 
   : StubBase(reinterpret_cast< void*>(ior)) { }

// Alternate constructor: does not call addRef()
// (sets d_weak_reference=isWeak)
// For internal use by Impls (fixes bug#275)
ucxx::sidl::rmi::ProtocolFactory::ProtocolFactory ( 
  ::ucxx::sidl::rmi::ProtocolFactory::ior_t* ior, bool isWeak ) : 
StubBase(reinterpret_cast< void*>(ior), isWeak){ 
}

// protected method that implements casting
void* ucxx::sidl::rmi::ProtocolFactory::_cast(const char* type) const
{
  ior_t* loc_self = reinterpret_cast< ior_t*>(this->d_self);
  void* ptr = 0;
  if ( loc_self != 0 ) {
    ptr = reinterpret_cast< void*>((*loc_self->d_epv->f__cast)(loc_self, type));
  }
  return ptr;
}

// Static data type
const ::ucxx::sidl::rmi::ProtocolFactory::ext_t * 
  ucxx::sidl::rmi::ProtocolFactory::s_ext = 0;

// private static method to get static data type
const ::ucxx::sidl::rmi::ProtocolFactory::ext_t *
ucxx::sidl::rmi::ProtocolFactory::_get_ext()
  throw ( ::ucxx::sidl::NullIORException)
{
  if (! s_ext ) {
    s_ext = sidl_rmi_ProtocolFactory__externals();
  }
  return s_ext;
}

