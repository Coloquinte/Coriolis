// -*- C++ -*-
//
// This file is part of the Hurricane Software.
// Copyright (c) BULL S.A. 2000-2018, All Rights Reserved
//
// +-----------------------------------------------------------------+
// |                  H U R R I C A N E                              |
// |     V L S I   B a c k e n d   D a t a - B a s e                 |
// |                                                                 |
// |  Author      :                       Remy Escassut              |
// |  E-mail      :            Jean-Paul.Chaput@lip6.fr              |
// | =============================================================== |
// |  C++ Header  :  "./hurricane/viewer/Selector.h"                 |
// +-----------------------------------------------------------------+


#ifndef  HURRICANE_SELECTOR_H
#define  HURRICANE_SELECTOR_H

#include  "hurricane/Property.h"
#include  "hurricane/Selectors.h"
#include  "hurricane/Occurrence.h"


namespace Hurricane {


  class CellWidget;


  struct SelectorLess {
      bool operator () ( const Selector* lhs, const Selector* rhs ) const;
  };


  class Selector : public PrivateProperty {

    public:
    // Constructor.
      static  Selector*         create           ( Occurrence& occurrence );
    // Methods.
      static  const Name&       getPropertyName  ();
      virtual Name              getName          () const;
              Occurrence        getOccurrence    () const;
      inline  set<CellWidget*>& getCellWidgetSet ();
              bool              isAttachedTo     ( CellWidget* ) const;
              void              attachTo         ( CellWidget* );
              void              detachFrom       ( CellWidget* , bool inDeletion=false );
    // Inspector Managment.
      virtual string            _getTypeName     () const;
      virtual string            _getString       () const;
      virtual Record*           _getRecord       () const;

    protected:
    // Internal: Attributes.
      static  const Name        _propertyName;
              set<CellWidget*>  _cellWidgets;

    protected:
    // Internal: Constructor.
                                Selector         ();
      virtual void              _preDestroy      ();
  };


// Inline Functions.
  inline set<CellWidget*>& Selector::getCellWidgetSet () { return _cellWidgets; }


  typedef set<Selector*,SelectorLess>  SelectorSet;


} // Hurricane namespace.

#endif  // HURRICANE_SELECTOR_H
