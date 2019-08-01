// -*- mode: C++; explicit-buffer-name: "Configuration.h<etesian>" -*-
//
// This file is part of the Coriolis Software.
// Copyright (c) UPMC 2014-2018, All Rights Reserved
//
// +-----------------------------------------------------------------+
// |                   C O R I O L I S                               |
// |   E t e s i a n  -  A n a l y t i c   P l a c e r               |
// |                                                                 |
// |  Author      :                    Jean-Paul CHAPUT              |
// |  E-mail      :            Jean-Paul.Chaput@lip6.fr              |
// | =============================================================== |
// |  C++ Header  :  "./etesian/Configuration.h"                     |
// +-----------------------------------------------------------------+


#ifndef  ETESIAN_CONFIGURATION_H
#define  ETESIAN_CONFIGURATION_H

#include  <string>

#include  "hurricane/DbU.h"
namespace Hurricane {
  class Layer;
  class Cell;
}

#include  "crlcore/RoutingGauge.h"
#include  "crlcore/CellGauge.h"


namespace Etesian {


  using  std::string;
  using  Hurricane::Record;
  using  Hurricane::Layer;
  using  Hurricane::DbU;
  using  Hurricane::Cell;
  using  CRL::CellGauge;
  using  CRL::RoutingGauge;


// -------------------------------------------------------------------
// Class  :  "Etesian::Configuration".

  enum Effort        { Fast    =1
                     , Standard=2
                     , High    =3
                     , Extreme =4
                     };
  enum GraphicUpdate { UpdateAll =1 
                     , LowerBound=2 
                     , FinalOnly =3 
                     };
  enum Density       { ForceUniform=1
                     , MaxDensity  =2
                     };

  class Configuration {
    public:
    // Constructor & Destructor.
                              Configuration    ( const RoutingGauge* rg=NULL, const CellGauge* cg=NULL );
                             ~Configuration    ();
             Configuration*   clone            () const;
    // Methods.
      inline RoutingGauge*    getGauge         () const;
      inline CellGauge*       getCellGauge     () const;
      inline Effort           getPlaceEffort   () const;
      inline GraphicUpdate    getUpdateConf    () const;
      inline Density          getSpreadingConf () const;
      inline bool             getRoutingDriven () const;
      inline double           getSpaceMargin   () const;
      inline double           getAspectRatio   () const;
      inline string           getFeedNames     () const;
      inline string           getBloat         () const;
             void             print            ( Cell* ) const;
             Record*          _getRecord       () const;
             string           _getString       () const;
             string           _getTypeName     () const;
    protected:
    // Attributes.
      RoutingGauge*  _rg;
      CellGauge*     _cg;
      Effort         _placeEffort;
      GraphicUpdate  _updateConf;
      Density        _spreadingConf;
      bool           _routingDriven;
      double         _spaceMargin;
      double         _aspectRatio;
      string         _feedNames;
      string         _bloat;
    private:
                             Configuration ( const Configuration& );
      Configuration& operator=             ( const Configuration& );
  };


  inline RoutingGauge* Configuration::getGauge         () const { return _rg; }
  inline CellGauge*    Configuration::getCellGauge     () const { return _cg; }
  inline Effort        Configuration::getPlaceEffort   () const { return _placeEffort; }
  inline GraphicUpdate Configuration::getUpdateConf    () const { return _updateConf; }
  inline Density       Configuration::getSpreadingConf () const { return _spreadingConf; }
  inline bool          Configuration::getRoutingDriven () const { return _routingDriven; }
  inline double        Configuration::getSpaceMargin   () const { return _spaceMargin; }
  inline double        Configuration::getAspectRatio   () const { return _aspectRatio; }
  inline string        Configuration::getFeedNames     () const { return _feedNames; }
  inline string        Configuration::getBloat         () const { return _bloat; }


} // Etesian namespace.

INSPECTOR_P_SUPPORT(Etesian::Configuration);

#endif  // ETESIAN_CONFIGURATION_H
