// -*- C++ -*-
//
// This file is part of the Coriolis Software.
// Copyright (c) UPMC 2008-2016, All Rights Reserved
//
// +-----------------------------------------------------------------+
// |                   C O R I O L I S                               |
// |      K i t e  -  D e t a i l e d   R o u t e r                  |
// |                                                                 |
// |  Author      :                    Jean-Paul CHAPUT              |
// |  E-mail      :       Jean-Paul.Chaput@asim.lip6.fr              |
// | =============================================================== |
// |  C++ Module  :       "./ProtectRoutingPads.cpp"                 |
// +-----------------------------------------------------------------+


#include <map>
#include <list>
#include "hurricane/DataBase.h"
#include "hurricane/Technology.h"
#include "hurricane/BasicLayer.h"
#include "hurricane/RegularLayer.h"
#include "hurricane/Horizontal.h"
#include "hurricane/Vertical.h"
#include "hurricane/RoutingPad.h"
#include "hurricane/Occurrence.h"
#include "hurricane/Cell.h"
#include "hurricane/NetExternalComponents.h"
#include "hurricane/NetRoutingProperty.h"
#include "crlcore/Catalog.h"
#include "anabatic/AutoContact.h"
#include "anabatic/AutoSegment.h"
#include "anabatic/GCell.h"
#include "anabatic/AnabaticEngine.h"
#include "katana/RoutingPlane.h"
#include "katana/TrackSegment.h"
#include "katana/TrackFixedSegment.h"
#include "katana/Track.h"
#include "katana/KatanaEngine.h"


namespace {

  using namespace std;
  using Hurricane::tab;
  using Hurricane::ForEachIterator;
  using Hurricane::DbU;
  using Hurricane::Box;
  using Hurricane::Interval;
  using Hurricane::Go;
  using Hurricane::Layer;
  using Hurricane::BasicLayer;
  using Hurricane::RegularLayer;
  using Hurricane::Horizontal;
  using Hurricane::Vertical;
  using Hurricane::Transformation;
  using Hurricane::RoutingPad;
  using Hurricane::Occurrence;
  using Hurricane::Path;
  using Hurricane::NetExternalComponents;
  using Hurricane::NetRoutingExtension;
  using Hurricane::NetRoutingState;
  using CRL::CatalogExtension;
  using Anabatic::AutoContact;
  using Anabatic::AutoSegment;
  using namespace Katana;


  void  protectRoutingPad ( RoutingPad* rp )
  {
    Name            padNetName     = "pad";
    Component*      usedComponent  = rp->_getEntityAsComponent();
    Path            path           = rp->getOccurrence().getPath();
    Net*            masterNet      = usedComponent->getNet();
    Transformation  transformation = path.getTransformation();

    if ( CatalogExtension::isPad(masterNet->getCell()) ) {
      if (   rp->getNet()->isPower()
         or (rp->getNet()->getName() == padNetName) )
        return;
    }

    vector<Segment*> segments;

    for( Segment* segment : masterNet->getSegments() ) {
      RoutingPlane* plane = Session::getKatanaEngine()->getRoutingPlaneByLayer(segment->getLayer());
      if ( plane == NULL ) continue;

      if ( usedComponent == dynamic_cast<Component*>(segment) ) continue;
      if ( not NetExternalComponents::isExternal(segment) ) continue;

    //cerr << "Looking " << (void*)*isegment << ":" << *isegment << endl;

      segments.push_back ( segment );
    }

    for ( size_t i=0 ; i<segments.size() ; ++i ) {
      RoutingPlane* plane     = Session::getKatanaEngine()->getRoutingPlaneByLayer(segments[i]->getLayer());
      Flags         direction = plane->getDirection();
      DbU::Unit     wireWidth = plane->getLayerGauge()->getWireWidth();
      DbU::Unit     delta     =   plane->getLayerGauge()->getHalfPitch()
                                + wireWidth/2
                                - DbU::fromLambda(0.1);
      DbU::Unit     extension = segments[i]->getLayer()->getExtentionCap();
      Box           bb        ( segments[i]->getBoundingBox() );

      transformation.applyOn ( bb );
    //cinfo << "bb: " << bb << endl;

      if ( direction == Flags::Horizontal ) {
        DbU::Unit axisMin = bb.getYMin() - delta;
        DbU::Unit axisMax = bb.getYMax() + delta;

        Track* track = plane->getTrackByPosition ( axisMin, Constant::Superior );
        for ( ; track and (track->getAxis() <= axisMax) ; track = track->getNextTrack() ) {
          Horizontal* segment = Horizontal::create ( rp->getNet()
                                                   , segments[i]->getLayer()
                                                   , track->getAxis()
                                                   , wireWidth
                                                   , bb.getXMin()+extension
                                                   , bb.getXMax()-extension
                                                   );
          TrackFixedSegment::create ( track, segment );
        }
      } else {
        DbU::Unit axisMin = bb.getXMin() - delta;
        DbU::Unit axisMax = bb.getXMax() + delta;

        Track* track = plane->getTrackByPosition ( axisMin, Constant::Superior );
        for ( ; track and (track->getAxis() <= axisMax) ; track = track->getNextTrack() ) {
          Vertical* segment = Vertical::create ( rp->getNet()
                                               , segments[i]->getLayer()
                                               , track->getAxis()
                                               , wireWidth
                                               , bb.getYMin()+extension
                                               , bb.getYMax()-extension
                                               );
          TrackFixedSegment::create ( track, segment );
        }
      }
    }
  }


} // End of anonymous namespace.


namespace Katana {

  using Hurricane::DataBase;
  using Hurricane::Technology;
  using Hurricane::BasicLayer;
  using Hurricane::Cell;
  using Anabatic::NetData;


  void  KatanaEngine::protectRoutingPads ()
  {
    cmess1 << "  o  Protect external components not useds as RoutingPads." << endl;

    openSession();

    for ( Net* net : getCell()->getNets() ) {
      if (net->isSupply()) continue;

      NetData* data = getNetData( net );
      if (data and data->isFixed()) continue;

      vector<RoutingPad*> rps;
      for ( RoutingPad* rp : net->getRoutingPads() ) {
        rps.push_back( rp );
      }

      for ( size_t i=0 ; i<rps.size() ; ++i )
        protectRoutingPad( rps[i] );
    }

    Session::close();
  }


}  // Katana namespace.
