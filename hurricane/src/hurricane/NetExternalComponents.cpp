
// -*- C++ -*-
//
// Copyright (c) BULL S.A. 2000-2009, All Rights Reserved
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
// |  Author      :                Christophe Alexandre              |
// |  E-mail      :            Jean-Paul.Chaput@lip6.fr              |
// | =============================================================== |
// |  C++ Module  :  "./NetExternalComponents.cpp"                   |
// | *************************************************************** |
// |  U p d a t e s                                                  |
// |                                                                 |
// x-----------------------------------------------------------------x


#include "hurricane/Error.h"
#include "hurricane/Net.h"
#include "hurricane/NetExternalComponents.h"


namespace Hurricane {


  const Name NetExternalComponents::_name = "ExternalComponentsRelation";


  StandardRelation* NetExternalComponents::getRelation ( const Net* net )
  {
    Property* property = net->getProperty(_name);
    if (!property) {
        return NULL;
    } else {
        StandardRelation* relation = dynamic_cast<StandardRelation*>(property);
        if (!relation)
            throw Error("Bad Property type: Must be a Standard Relation");
        return relation;
    }
  }


  Components NetExternalComponents::get ( const Net* net )
  {
    if (!net->isExternal())
      throw Error("Impossible to retrieve external components on non external net "
                 + net->getName()._getString());
    
    StandardRelation* externalComponentsRelation = getRelation(net);
    if (!externalComponentsRelation)
      return Components();
    return externalComponentsRelation->getSlaveOwners().getSubSet<Component*>();
  }


  void NetExternalComponents::setExternal ( Component* component )
  {
    Net* net = component->getNet();
    if (!net->isExternal())
      throw Error("Impossible to set as external a component member of non external net "
                 + net->getName()._getString());
    StandardRelation* externalComponentsRelation = getRelation(net);
    if (!externalComponentsRelation)
      externalComponentsRelation = StandardRelation::create(net, _name);
    component->put(externalComponentsRelation);
  }


  bool  NetExternalComponents::isExternal ( Component* component )
  {
    Net* net = component->getNet();
    if (!net->isExternal()) return false;

    return getRelation(net) != NULL;
  }


} // End of Hurricane namespace.
