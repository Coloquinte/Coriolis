// -*- C++ -*-
//
// This file is part of the Coriolis Software.
// Copyright (c) Sorbonne Universite 2019-2019, All Rights Reserved
//
// +-----------------------------------------------------------------+
// |                   C O R I O L I S                               |
// |   E t e s i a n  -  A n a l y t i c   P l a c e r               |
// |                                                                 |
// |  Author      :                    Jean-Paul CHAPUT              |
// |  E-mail      :            Jean-Paul.Chaput@lip6.fr              |
// | =============================================================== |
// |  C++ Module  :       "./BloatCells.cpp"                         |
// +-----------------------------------------------------------------+


#include "hurricane/Error.h"
#include "hurricane/Warning.h"
#include "hurricane/Cell.h"
#include "etesian/EtesianEngine.h"


namespace Etesian {

  using std::cerr;
  using std::endl;
  using Hurricane::Error;
  using Hurricane::Warning;
  using Hurricane::DbU;


  BloatCell::BloatCell ( std::string name )
    : _name(name)
  { }


  BloatCell::~BloatCell ()
  { }


  BloatKey::BloatKey ( string key )
    : BloatCell(key)
  { }


  BloatKey::~BloatKey ()
  { }


  DbU::Unit  BloatKey::getDx ( const Cell*, const EtesianEngine* ) const
  {
    cerr << Error( "BloatKey::getAb() must never be called (\"%s\").", getName().c_str() ) << endl;
    return 0;
  }  


  BloatDisabled::BloatDisabled ()
    : BloatCell("disabled")
  { }


  BloatDisabled::~BloatDisabled ()
  { }


  DbU::Unit  BloatDisabled::getDx ( const Cell* cell, const EtesianEngine* ) const
  { return 0; }  


  BloatNsxlib::BloatNsxlib ()
    : BloatCell("nsxlib")
  { }


  BloatNsxlib::~BloatNsxlib ()
  { }


  DbU::Unit  BloatNsxlib::getDx ( const Cell* cell, const EtesianEngine* etesian ) const
  {
    Box ab ( cell->getAbutmentBox() );
    DbU::Unit vpitch = etesian->getVerticalPitch();;
    int       xsize  = (ab.getWidth() + vpitch - 1) / vpitch;

    if (xsize < 6) return vpitch*2;
    
    return 0;
  }  


  bool  BloatCells::select ( std::string profile )
  {
    BloatKey key ( profile );
    
    auto ibloat = _bloatCells.find( &key );
    if (ibloat != _bloatCells.end()) {
      _selected = *ibloat;
      return true;
    }

    cerr << Warning( "BloatCells::select(): No profile named \"%s\", using \"disabled\"."
                   , profile.c_str()
                   ) << endl;

    return select( "disabled" );
  }

  
  Box  BloatCells::getAb ( const Cell* cell )
  {
    DbU::Unit dx = _selected->getDx( cell, _etesian );

    _dxSpace += dx;
    Box ab = cell->getAbutmentBox();
    return ab.inflate( 0, 0, dx, 0 );
  }


}  // Etesian namespace.
