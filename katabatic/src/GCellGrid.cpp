// -*- C++ -*-
//
// This file is part of the Coriolis Software.
// Copyright (c) UPMC 2008-2015, All Rights Reserved
//
// +-----------------------------------------------------------------+
// |                   C O R I O L I S                               |
// |        K a t a b a t i c  -  Routing Toolbox                    |
// |                                                                 |
// |  Author      :                    Jean-Paul CHAPUT              |
// |  E-mail      :            Jean-Paul.Chaput@lip6.fr              |
// | =============================================================== |
// |  C++ Module  :       "./GCellGrid.cpp"                          |
// +-----------------------------------------------------------------+


#include  "hurricane/Error.h"
#include  "hurricane/Cell.h"
#include  "spaghetti/SpaghettiEngine.h"
#include  "katabatic/Session.h"
#include  "katabatic/GCellGrid.h"
#include  "katabatic/KatabaticEngine.h"


namespace Katabatic {


  using namespace std;
  using Hurricane::tab;
  using Hurricane::ltracein;
  using Hurricane::ltraceout;
  using Hurricane::inltrace;
  using Hurricane::Error;
  using Hurricane::ForEachIterator;
  using spaghetti::SpaghettiEngine;


// -------------------------------------------------------------------
// Class  :  "Katabatic::GCellGrid".


  GCellGrid::GCellGrid ( KatabaticEngine* ktbt )
    : Grid<GCell>   (ktbt->getCell()->getAbutmentBox())
    , _katabatic    (ktbt)
    , _densityMode  (MaxDensity)
    , _hEdgeCapacity(ktbt->getConfiguration()->getHEdgeCapacity())
    , _vEdgeCapacity(ktbt->getConfiguration()->getVEdgeCapacity())
  { }


  void  GCellGrid::_postCreate ()
  {
    SpaghettiEngine* globalRouter = SpaghettiEngine::get ( getCell() );
    if ( !globalRouter )
      throw Error ( "%s :\n\n    Hey, Banana! You forgot to run the global router on %s.\n", "GCellGrid::_postCreate()", getString(getCell()).c_str() );

    vector<DbU::Unit> grHGraduations = globalRouter->getHorizontalCutLines ();
    for ( size_t i=0 ; i<grHGraduations.size() ; i++ )
      _yGraduations.addGraduation ( grHGraduations[i] );
    _yGraduations.sort ();
    _rows = _yGraduations.getSize() - 1;

    vector<DbU::Unit> grVGraduations = globalRouter->getVerticalCutLines ();
    for ( size_t i=0 ; i<grVGraduations.size() ; i++ )
      _xGraduations.addGraduation ( grVGraduations[i] );
    _xGraduations.sort ();
    _columns = _xGraduations.getSize() - 1;

    _rawSize = _columns * _rows;

    ltrace(80) << "Katabatic GCell Matrix [" << getColumns() << "x" << getRows() << "]" << endl;
    ltracein(80);
    ltrace(80) << "_xGraduations := " << _xGraduations._print() << endl;
    ltrace(80) << "_yGraduations := " << _yGraduations._print() << endl;

    for ( unsigned int row=0 ; row < getRows() ; row++ ) {
      for ( unsigned int column=0; column<getColumns() ; column++ ) {
        Box boundingBox = Box ( _xGraduations[column  ]
                              , _yGraduations[row     ]
                              , _xGraduations[column+1]
                              , _yGraduations[row   +1]
                              );

        _gcells.push_back ( GCell::create ( this, _gcells.size(), boundingBox ) );
  
        ltrace(80) << "GCell [" << column << "," << row << "] "
                   << _gcells[_gcells.size()-1] << endl;
      }
    }

    cmess1 << "  o  Reading GCell grid from Spaghetti ["
           << getColumns() << "x" << getRows() << "] "
           << "(" << (getColumns()*getRows()) << ")." << endl;

    ltraceout(80);
  }


  GCellGrid* GCellGrid::create ( KatabaticEngine* ktbt )
  {
    GCellGrid* grid = new GCellGrid ( ktbt );
    grid->_postCreate ();
    return grid;
  }


  GCellGrid::~GCellGrid ()
  { }


  void  GCellGrid::_preDestroy ()
  {
    ltrace(90) << "GCellGrid::_preDestroy()" << endl;
    ltracein(90);

    vector<GCell*>::iterator   it = _gcells.begin();
    vector<GCell*>::iterator  end = _gcells.end  ();
    for ( ; it != end ; it++ ) (*it)->destroy ();

    ltraceout(90);
  }


  Cell* GCellGrid::getCell () const
  {
    return _katabatic->getCell();
  }


  Interval  GCellGrid::getUSide ( unsigned int direction ) const
  {
    Interval side;
    switch ( direction ) {
      default:
      case Constant::Horizontal: side = Interval(_boundingBox.getXMin(),_boundingBox.getXMax()); break;
      case Constant::Vertical:   side = Interval(_boundingBox.getYMin(),_boundingBox.getYMax()); break;
    }
    return side;
  }


  void  GCellGrid::updateContacts ( unsigned int flags )
  {
    if (flags & KbOpenSession) Session::open( _katabatic );

    forEach ( GCell*, gcell, getGCells() )
      gcell->updateContacts();

    if (flags & KbOpenSession) Session::close();
  }


  size_t  GCellGrid::updateDensity ()
  {
    size_t  saturateds = 0;
    forEach ( GCell*, gcell, getGCells() ) {
      saturateds += gcell->updateDensity ();
    }
    return saturateds;
  }


  size_t  GCellGrid::checkDensity () const
  {
    size_t  saturateds = 0;
    forEach ( GCell*, gcell, const_cast<GCellGrid*>(this)->getGCells() ) {
      saturateds += gcell->checkDensity ();
    }
    return saturateds;
  }


  bool  GCellGrid::checkEdgeOverflow ( size_t hreserved, size_t vreserved) const
  {
    bool overload = false;
    forEach ( GCell*, gcell, const_cast<GCellGrid*>(this)->getGCells() ) {
      overload = gcell->checkEdgeSaturation(hreserved,vreserved) or overload;
    }
    return overload;
  }


  string  GCellGrid::_getTypeName () const
  {
    return _TName("GCellGrid");
  }


  string  GCellGrid::_getString () const
  {
    return "<" + _getTypeName() + " "
               + getString(getRows()) + "x" + getString(getColumns()) + ">";
  }


  void  GCellGrid::_xmlWrite ( ostream& o )
  {
    updateDensity ();

    o << "<GCellGrid Cell=\"" << getCell()->getName() << "\">" << endl; 
    o << "<!--            GRID           Lower Left Corner            Density    -->" << endl;
    o << "<!--        column row             XMin YMin             hor  ver  con -->" << endl;

    unsigned int  row = 0;
    forEach ( GCell*, gcell, getGCells() ) {
      if ( gcell->getRow() > row ) {
        o << endl;
        o << "<!--        column row             XMin YMin             hor  ver  con -->" << endl;
        row = gcell->getRow();
      }

      o << "  "; gcell->_xmlWrite ( o ); o << endl;
    }

    o << "</GCellGrid>" << endl;
  }


  Record* GCellGrid::_getRecord () const
  {
    Record* record = Grid<GCell>::_getRecord ();
    record->add ( getSlot ( "_katabatic", _katabatic ) );
    return record;
  }



} // End of Katabatic namespace.
