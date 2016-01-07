// ****************************************************************************************************
// File: ./DataBase.cpp
// Authors: R. Escassut
// Copyright (c) BULL S.A. 2000-2015, All Rights Reserved
//
// This file is part of Hurricane.
//
// Hurricane is free software: you can redistribute it  and/or  modify it under the  terms  of the  GNU
// Lesser General Public License as published by the Free Software Foundation, either version 3 of  the
// License, or (at your option) any later version.
//
// Hurricane is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without  even
// the implied warranty of MERCHANTABILITY or FITNESS FOR A  PARTICULAR  PURPOSE. See  the  Lesser  GNU
// General Public License for more details.
//
// You should have received a copy of the Lesser GNU General Public License along  with  Hurricane.  If
// not, see <http://www.gnu.org/licenses/>.
// ****************************************************************************************************

#include "hurricane/DataBase.h"
#include "hurricane/SharedPath.h"
#include "hurricane/Technology.h"
#include "hurricane/Library.h"
#include "hurricane/Error.h"
#include "hurricane/UpdateSession.h"

namespace Hurricane {



// ****************************************************************************************************
// DataBase implementation
// ****************************************************************************************************

DataBase* DataBase::_db = NULL;

DataBase::DataBase()
// *****************
:    Inherit(),
    _technology(NULL),
    _rootLibrary(NULL)
{
    if (_db)
        throw Error("Can't create " + _TName("DataBase") + " : already exists");
}

DataBase* DataBase::create()
// *************************
{
    DataBase* dataBase = new DataBase();

    dataBase->_postCreate();

    return dataBase;
}

void DataBase::_postCreate()
// *************************
{
    Inherit::_postCreate();

    _db = this;
}

void DataBase::_preDestroy()
// ************************
{
    UpdateSession::open();
    Inherit::_preDestroy();

    if (_rootLibrary) _rootLibrary->destroy();
    if (_technology) _technology->destroy();
    UpdateSession::close();

    _db = NULL;
}

string DataBase::_getString() const
// ********************************
{
    return Inherit::_getString();
}

Record* DataBase::_getRecord() const
// *********************************
{
    Record* record = Inherit::_getRecord();
    if (record) {
        record->add(getSlot("_technology"    , _technology        ));
        record->add(getSlot("_rootLibrary"   , _rootLibrary       ));
        record->add(getSlot("DbU::precision" , DbU::getPrecision()));
        record->add(getSlot("DbU::resolution", DbU::db(1)         ));
      //record->add(getSlot("GridStep", getValueString(getGridStep())));
    }
    return record;
}

DataBase* DataBase::getDB()
// ************************
{
    return _db;
}

Library* DataBase::getLibrary(string rpath) const
// **********************************************
{
  Library* current  = getRootLibrary();
  if (not current) return NULL;

  char   separator = SharedPath::getNameSeparator();
  Name   childName;
  size_t dot       = rpath.find( separator );
  if (dot != string::npos) {
    childName = rpath.substr( 0, dot );
    rpath     = rpath.substr( dot+1 );
  } else
    childName = rpath;

  if (childName != current->getName())
    return NULL;

  while ( dot != string::npos ) {
    dot = rpath.find( separator );
    if (dot != string::npos) {
      childName = rpath.substr( 0, dot );
      rpath     = rpath.substr( dot+1 );
    } else
      childName = rpath;

    current = current->getLibrary( childName );
  }
  return current;
}

Cell* DataBase::getCell(string rpath) const
// ****************************************
{

  char     separator = SharedPath::getNameSeparator();
  size_t   dot       = rpath.rfind( separator );
  string   cellName  = rpath.substr(dot+1);
  Library* library   = getLibrary( rpath.substr(0,dot) );
  Cell*    cell      = NULL;

  if (library)
    cell = library->getCell( rpath.substr(dot+1) );

  if (not cell and _cellLoader) return _cellLoader( rpath );

  return cell;
  
}

} // End of Hurricane namespace.


// ****************************************************************************************************
// Copyright (c) BULL S.A. 2000-2015, All Rights Reserved
// ****************************************************************************************************
