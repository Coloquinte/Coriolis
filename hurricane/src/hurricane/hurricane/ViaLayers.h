
// -*- C++ -*-
//
// Copyright (c) BULL S.A. 2000-2018, All Rights Reserved
//
// This file is part of Hurricane.
//
// Hurricane is free software: you can redistribute it  and/or  modify
// it under the terms of the GNU  Lesser  General  Public  License  as
// published by the Free Software Foundation, either version 3 of  the
// License, or (at your option) any later version.
//
// Hurricane is distributed in the hope that it will  be  useful,  but
// WITHOUT ANY WARRANTY; without even the implied warranty of MERCHAN-
// TABILITY or FITNESS FOR A PARTICULAR PURPOSE. See  the  Lesser  GNU
// General Public License for more details.
//
// You should have received a copy of the Lesser  GNU  General  Public
// License along with Hurricane. If not, see
//                                     <http://www.gnu.org/licenses/>.
//
// ===================================================================
//
// $Id$
//
// x-----------------------------------------------------------------x
// |                                                                 |
// |                  H U R R I C A N E                              |
// |     V L S I   B a c k e n d   D a t a - B a s e                 |
// |                                                                 |
// |  Author      :                    Jean-Paul Chaput              |
// |  E-mail      :            Jean-Paul.Chaput@lip6.fr              |
// | =============================================================== |
// |  C++ Header  :  "./hurricane/ViaLayers.h"                       |
// | *************************************************************** |
// |  U p d a t e s                                                  |
// |                                                                 |
// x-----------------------------------------------------------------x


#ifndef __HURRICANE_VIA_LAYERS_H__
#define __HURRICANE_VIA_LAYERS_H__


#include  "hurricane/Collection.h"


namespace Hurricane {


  class ViaLayer;


  typedef GenericCollection<ViaLayer*> ViaLayers;
  typedef GenericLocator<ViaLayer*>    ViaLayerLocator;
  typedef GenericFilter<ViaLayer*>     ViaLayerFilter;

#define  for_each_via_layer(viaLayer, viaLayers) {    \
   ViaLayerLocator _locator = viaLayers.getLocator(); \
   while ( _locator.isValid() ) {                     \
       ViaLayer* viaLayer = _locator.getElement();    \
       _locator.progress();

}


#endif  // __HURRICANE_VIA_LAYERS__
