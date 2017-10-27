// -*- C++ -*-
//
// This file is part of the Coriolis Software.
// Copyright (c) UPMC 2008-2016, All Rights Reserved
//
// +-----------------------------------------------------------------+
// |                   C O R I O L I S                               |
// |         A n a b a t i c  -  Routing Toolbox                     |
// |                                                                 |
// |  Author      :                    Jean-Paul CHAPUT              |
// |  E-mail      :            Jean-Paul.Chaput@lip6.fr              |
// | =============================================================== |
// |  C++ Module  :       "./LoadGlobalRouting.cpp"                  |
// +-----------------------------------------------------------------+


#include <cstdlib>
#include <sstream>
#include "hurricane/Bug.h"
#include "hurricane/Breakpoint.h"
#include "hurricane/Error.h"
#include "hurricane/Warning.h"
#include "hurricane/DebugSession.h"
#include "hurricane/Layer.h"
#include "hurricane/BasicLayer.h"
#include "hurricane/RegularLayer.h"
#include "hurricane/Technology.h"
#include "hurricane/DataBase.h"
#include "hurricane/Net.h"
#include "hurricane/NetExternalComponents.h"
#include "hurricane/NetRoutingProperty.h"
#include "hurricane/RoutingPad.h"
#include "hurricane/RoutingPads.h"
#include "hurricane/Pad.h"
#include "hurricane/Plug.h"
#include "hurricane/Cell.h"
#include "hurricane/Instance.h"
#include "hurricane/Vertical.h"
#include "hurricane/Horizontal.h"
#include "crlcore/AllianceFramework.h"
#include "crlcore/RoutingGauge.h"
#include "crlcore/Measures.h"
#include "anabatic/AutoContactTerminal.h"
#include "anabatic/AutoContactTurn.h"
#include "anabatic/AutoContactHTee.h"
#include "anabatic/AutoContactVTee.h"
#include "anabatic/AutoSegment.h"
#include "anabatic/AnabaticEngine.h"


namespace {

  using Anabatic::AutoContactTerminal;
  using Hurricane::Breakpoint;
  


/*! \defgroup     LoadGlobalRouting  Global Routing Loading
 *  \brief        Translation rules to build detailed routing from global
 *
 *                This module documents how the global routing built by \c Knik is
 *                loaded into the \c Anabatic data-base. It is intented for developpers
 *                only.
 */

//! \addtogroup   LoadGlobalRouting
//! \{

//! \enum         LocalFunctionFlag
//!               A set of flags for all functions of the LoadGlobalRouting module.
//!               They can be combined to form the \e flags argument of functions.
//!               the functions will ignore flags that are not intended to them.
//!
//!               For \c HSmall, \c VSmall & \c Punctual see checkRoutingPadSize().

//! \var          LocalFunctionFlag::NoFlags
//!               A simple alias over zero to explicitly tell that no flag at all is
//!               passed to the function.

//! \var          LocalFunctionFlag::HAccess
//!               The constructed topology will be accessed through an horizontal
//!               segment. The absence of this flag tell that the access will be done
//!               trough a vertical.

//! \var          LocalFunctionFlag::VSmall
//!               The RoutingPad vertically covers a very small number of access points,
//!               so it is likely overconstrained for direct horizontal connexion.

//! \var          LocalFunctionFlag::HSmall
//!               The RoutingPad horizontally covers a very small number of access points,
//!               so it is likely overconstrained for direct vertical connexion.

//! \var          LocalFunctionFlag::Punctual
//!               The RoutingPad covers only an access point in either direction.

//! \var          LocalFunctionFlag::DoSourceContact
//!               When creating Anabatic::AutoContactTerminal on non-punctual RoutingPad, this flag
//!               request the creation of a contact <em>on the source point</em>.

//! \var          LocalFunctionFlag::DoTargetContact
//!               When creating Anabatic::AutoContactTerminal on non-punctual RoutingPad, this flag
//!               request the creation of a contact <em>on the target point</em>.


//! \function     uint64_t  checkRoutingPadSize ( Component* rp );
//!
//!               Look at the geometrical size of the Component and assess if
//!               it's span is too narrow either horizontally or vertically.
//!               Return a combination of flags indicating it's state:
//!                 - HSmall : less than 3 pitches in horizontal direction.
//!                 - VSmall : less than 3 pitches in vertical direction.
//!                 - Punctual : one pitch in either directions.
//!
//!               The component can be a RoutingPad, a Vertical or an Horizontal.
//!
//!               \image html  checkRoutingPadSize.png "checkRoutingPadSize()"

/*! \class        GCellTopology
 *
 *  \brief        Build the wiring for a Net inside a GCell (\b internal).
 *
 *                As this class is called to initially construct the Anabatic wiring,
 *                it must build a \b connex wiring. That is without gaps in layer depth,
 *                because the topology restauration mechanism (AutoContact::updateTopology())
 *                of the AutoContact cannot work until all AutoSegments are revalidated at
 *                least once. The topology restauration work by creating doglegs which in turn,
 *                call the canonization, which needs all the caches to be up to date.
 */

//! \function     void  GCellTopology::doRp_AutoContacts ( GCell* gcell, Component* rp, AutoContact*& source, AutoContact*& target, uint64_t flags );
//! \param        gcell   The GCell into which create the AutoContact.
//! \param        rp      The Component we want to access.
//! \param        source  The AutoContact created on the \c source (\e returned).
//! \param        target  The AutoContact created on the \c target (\e returned).
//! \param        flags   Managed by this function:
//!                         - LocalFunctionFlag::DoSourceContact
//!                         - LocalFunctionFlag::DoTargetContact
//!
//!               Create the AutoContact directly anchored on the Component (terminal).
//!               Three cases are manageds:
//!                 -# <b>Ordinary (non-punctual) \c METAL1 terminal</b>: an AutoContactTerminal
//!                    is anchored on the RoutingPad.
//!                 -# <b>Punctual \c METAL1 terminal</b>, the access must never be blocked
//!                    by other routing. To ensure it, we create a fixed AutoSegment (anchored
//!                    on two AutoContactTerminal) to cover it. The \e normal AutoContactTerminal
//!                    is also created.
//!                 -# <b>non \c METAL1 terminal</b>, as for the punctual \c METAL1, a
//!                    fixed protection is added over the RoutingPad. If we access
//!                    horizontally a vertical RoutingPad or vertically an horizontal
//!                    one, an extra AutoContactTerminal is added (to allow is displacement
//!                    along the RoutingPad).
//!
//!               To avoid creating a fixed protection over a RoutingPad multiple times,
//!               the RoutingPad and it's associated protection is stored in a static
//!               \c map : \c __routingPadAutoSegments.
//!
//!               Conversely, because an AutoContactTerminal can only be connected to one
//!               segment, each time this function is called a new terminal will be created
//!               (or maybe two in case of non-punctual terminals). If only one AutoContact
//!               is requested, it is created centered on the RoutingPad. The initial
//!               position of AutoContact <em>do not prevent them to move afterwards</em>,
//!               even those created on source/target on a non-punctual RoutingPad.
//!
//! \remark       For clarity we describe the layer management of this function in term
//!               of \c METAL, but it is the RoutingGauge depth which is actually used.
//!
//!               \image html  doRp_AutoContacts.png "doRp_AutoContacts()"

//! \function     AutoContact* GCellTopology::doRp_Access ( GCell* gcell, Component* rp, uint64_t flags );
//! \param        gcell   The GCell into which create the AutoContact.
//! \param        rp      The Component onto which anchor the access contact.
//! \param        flags   Relevant flags are:
//!                         - HAccess, the terminal is to be accessed through an horizontal
//!                           segment.
//!                         - VSmall, force the terminal to be considered as small in the
//!                           vertical direction.
//!
//!               If \c HAccess is set, the Component is to be accessed trough an horizontal
//!               segment. If unset, the access is done vertically.
//!
//!               Create an AutoContact to access a Component (terminal). If the Component
//!               is not to be accessed through an horizontal segment, and do not cover a
//!               large span in the horizontal direction (flag \c VSmall), a local horizontal
//!               AutoSegment is added to slacken the vertical constraints.
//!
//!               \image html  doRp_Access.png "doRp_Access()"

//! \function     AutoContact*  GCellTopology::doRp_AccessPad ( RoutingPad* rp, uint64_t flags );
//! \param        rp      The Component onto which anchor the access contact.
//! \param        flags   Relevant flags are:
//!                         - HAccess, the terminal is to be accessed through an horizontal
//!                           segment.
//!                         - VSmall, force the terminal to be considered as small in the
//!                           vertical direction.
//! \return       A Anabatic::AutoContactTerminal .
//!
//!               The Component \c rp is a RoutingPad which belongs to a pad cell. This case
//!               occurs when we are routing a complete chip. This method build, from the
//!               \c rp a stack of articulated punctual segments and contacts to reach the
//!               default H/V routing layers (usually \c METAL2 & \c METAL3). This may be
//!               needed when the pad terminal is in \c METAL5, for instance.
//!
//!               The returned AutoContactTerminal is anchored on the last punctual segment
//!               build.
//!
//!               The GCell into which the AutoContactTerminal is created may be under the
//!               pads area. However, it will be right on the border of the GCell.
//!               The global router vertexes of GCell under the pad area are marked as
//!               blocked so will never be used for routing.
//!
//! \remark       The segments and contacts added to ensure the layer connexity are not
//!               put into the Anabatic database. They are plain Hurricane objects, invisibles
//!               from it.

//! \function     void  GCellTopology::doRp_StairCaseH ( GCell* gcell, Component* rp1, Component* rp2 );
//!
//!               Build the wiring to connect to horizontal Component. Two cases:
//!                 - The Component are aligneds, then only a straight wire is created.
//!                 - They are \e not aligned, then a complete dogleg is created.
//!
//!               \image html  doRp_StairCaseH.png "doRp_StairCaseH()"

//! \function     void  GCellTopology::doRp_StairCaseV ( GCell* gcell, Component* rp1, Component* rp2 );
//!
//!               Build the wiring to connect to vertical Components. Two cases:
//!                 - The Components are aligneds, then only a straight wire is created.
//!                 - They are \e not aligned, then a complete dogleg is created.
//!
//!               \image html  doRp_StairCaseV.png "doRp_StairCaseV()"

//! \function     void  GCellTopology::_do_xG_1Pad ();
//!
//!               Construct the topology, when there is only global wires and one local
//!               terminal, but coming from a Pad. As thoses connectors will always be
//!               on one border of the GCell they can be considered as a kind of global.
//!
//!               So this method mostly calls GCellTopology::doRp_AccessPad() to create
//!               the AutoContactTerminal, then calls GCellTopology::_do_xG(), except
//!               for straight lines which are managed directly.

//! \function     void  GCellTopology::_do_xG ();
//!
//!               Construct the topology, when there is only global wires (no local terminals).
//!               
//!               Some topology are not handled because they must not be managed by this
//!               function:
//!               <ul>
//!                 <li>One global: nonsensical because there also must be a terminal.
//!                 <li>Two aligned globals: in that case we do a straight wire whithout
//!                     any AutoContact (handled by the source/target of the wire).
//!               </ul>
//!
//!               \image html  _do_xG.png "_do_xG()"

//! \function     void  GCellTopology::_do_1G_1M1 ();
//!
//!               Construct a topology where there is \e one global and one RoutingPad
//!               in \c METAL1. The \c METAL1 is assumed to be vertical.
//!
//! \remark       When accessing the RoutingPad through an horizontal global segment
//!               and the vertical extension of the segment is small, the global is
//!               still directly attached to the terminal, inducing a high constraint
//!               on it. We left to job of slackening it to the router.
//!
//!               \image html  _do_1G_1M1.png "_do_1G_1M1()"

//! \function     void  GCellTopology::_do_1G_xM1 ();
//!
//!               Construct a topology where there is \e one global and any number of
//!               RoutingPad in \c METAL1. The \c METAL1 is assumed to be vertical.
//!
//!               The RoutingPads are linked together two by two. If the horizontal
//!               segments are not aligned by the router, part of the routage will be
//!               done through the RoutingPad itself. The global incoming segment will
//!               connected to the leftmost, rightmost or centermost RoutingPad according
//!               from wich side it comes from.
//!
//!               \image html  _do_1G_xM1.png "_do_1G_xM1()"

//! \function     void  GCellTopology::_do_xG_1M1_1M2 ();
//!
//!               Construct a topology where there is at least one global (and up to 4),
//!               one \c METAL1 RoutingPad (assumed V) and one \c METAL2 RoutingPad (assumed H).
//!
//!               In this topology, we want to try to reuse the \c METAL2 RoutingPad as a
//!               feedtrough in the horizontal routage. Thus:
//!                 - The \c METAL1 and \c METAL2 RoutingPad are connected through a separate wiring.
//!                 - The south & west global wiring is attached to the leftmost contact of
//!                   the \c METAL2.
//!                 - The north & east global wiring is attached to the rightmost contact of
//!                   the \c METAL2.
//!
//!               South/west and north/south can be build independantly. Depending on the number
//!               of globals, they can consist of:
//!                 - Nothing (no south nor west).
//!                 - An AutoContact (west present).
//!                 - An horizontal plus a turn (south present).
//!                 - An horizontal plus a HTee (south & west present).
//!
//! \remark       Not all configurations are represented below.
//!
//!               \image html  _do_xG_1M1_1M2.png "_do_xG_1M1_1M2()"

//! \function     void  GCellTopology::_do_xG_xM1_xM3 ();
//!
//!               Construct a topology where there is at least one global (and up to 4),
//!               at least one \c METAL1 RoutingPad (assumed V) and at least one \c METAL3
//!               RoutingPad (assumed V).
//!
//!               In this topology, we want to try to reuse the \c METAL3 RoutingPad as a
//!               feedtrough in the vertical routage. Thus:
//!                 - The \c METAL1 and \c METAL3 RoutingPad are connected through a separate
//!                   wiring made of separate horizontals.
//!                 - The south-west global wiring is attached to the leftmost RoutingPad
//!                   if there isn't south or to the first \c METAL3 otherwise.
//!                 - The north-east global wiring is attached to the rightmost RoutingPad
//!                   if there isn't north or to the first \c METAL3 otherwise.
//!
//!               South/west and north/south can be build independantly. Depending on the number
//!               of globals, they can consist of:
//!                 - Nothing (no south nor west).
//!                 - An AutoContact on the leftmost RoutingPad (west present).
//!                 - An AutoContact on the first \c METAL3 (only south present).
//!                 - An AutoContact plus a vertical plus a VTee (south & west present).
//!
//!               \image html  _do_xG_xM1_xM3.png "_do_xG_xM1_xM3()"

//! \function     void  GCellTopology::_do_xG_xM2 ();
//!
//!               Construct a topology where there is at least one global (and up to 4),
//!               and any number of \c METAL2 RoutingPads (assumeds H).
//!
//!               In this topology, we want to try to reuse the \c METAL2 RoutingPad as a
//!               feedtrough in the horizontal routage. Thus:
//!                 - The RoutingPad are connecteds trough a separate staircase (or
//!                   straight wire if aligneds).
//!                 - The south-west global wiring is attached to the leftmost RoutingPad
//!                   if there isn't south or to the biggest horizontal RoutingPad otherwise.
//!                 - The north-east global wiring is attached to the rightmost RoutingPad
//!                   if there isn't south or to the biggest horizontal RoutingPad otherwise.
//!
//!               \image html  _do_xG_xM2.png "_do_xG_xM2()"

//! \function     void  GCellTopology::_do_1G_1M3 ();
//!
//!               Construct a topology where there is one global and one \c METAL3 RoutingPad
//!               (assumeds V).
//!
//!               In this topology, we reuse the \c METAL3 RoutingPad as a feedtrough in the
//!               vertical routage. Thus:
//!                 - If the global is either north or south, we directly connect to the
//!                   north end or south end of the RoutingPad. \red{The vertical global will}
//!                   \red{have no slack at all we assume that METAL3 terminals are only from}
//!                   \red{blocks and are aligneds vertically.}
//!                 - If the global is east or west \e and the RoutingPad is sufficiently
//!                   extended in the vertical direction, we connect an horizontal in the
//!                   normal way.
//!                 - If the global is not sufficiently extended, we add a turn to give some
//!                   slack to the global.
//!
//!
//!               \image html  _do_1G_1M3.png "_do_1G_1M3()"

//! \function     void  GCellTopology::_do_xG_xM3 ();
//!
//!               Construct a topology where there at least one global and two \c METAL3 RoutingPad
//!               (assumed V).
//!
//!               In this topology, we reuse the \c METAL3 RoutingPad as a feedtrough in the
//!               vertical routage. \red{We assume that the most likely relative position}
//!               \red{of the RoutingPads is to be aligned vertically}.
//!               Thus:
//!                 - All RoutingPads are linked two by two trough vertical staircases.
//!                 - The south-west global wiring is attached to the bottommost RoutingPad
//!                   (without vertical slack). If a misalignment is detected, then a
//!                   dogleg is added.
//!                 - The north-east global wiring is attached to the topmost RoutingPad
//!                   (without vertical slack).
//!
//!               South/west and north/south can be build independantly. Depending on the number
//!               of globals, they can consist of:
//!                 - Nothing (no south nor west).
//!                 - An sliding AutoContact on the bottommost RoutingPad (west present).
//!                 - An fixed AutoContact on the bottommost RoutingPad (only south present).
//!                 - An fixed AutoContact plus a vertical plus a VTee (south & west present).
//!
//!               \image html  _do_xG_xM3.png "_do_xG_xM3()"

//! \function     void  singleGCell ( AnabaticEngine* anbt, Net* net );
//!
//!               All the RoutingPads of the net are concentrated under a single
//!               GCell. This function assumes that all the terminals are in
//!               \c METAL1 (vertical), and link them two by two by horizontal
//!               wires.

//! \}


  using namespace std;
  using namespace CRL;
  using namespace Hurricane;
  using namespace Anabatic;


  // ---------------------------------------------------------------
  // Local Enum/Types.

  enum LocalFunctionFlag { NoFlags         = (1 <<  0)
                         , SortDecreasing  = (1 <<  1)
                         , HAccess         = (1 <<  2)
                         , VSmall          = (1 <<  3)
                         , HSmall          = (1 <<  4)
                         , Punctual        = (1 <<  5)
                         , HCollapse       = (1 <<  6)
                         , VCollapse       = (1 <<  7)
                         , Terminal        = (1 <<  8)
                         , DoSourceContact = (1 <<  9)
                         , DoTargetContact = (1 << 10)
                         , SouthBound      = (1 << 11)
                         , NorthBound      = (1 << 12)
                         , WestBound       = (1 << 13)
                         , EastBound       = (1 << 14)
                         , Middle          = (1 << 15)
                         , SouthWest       = SouthBound|WestBound
                         , NorthEast       = NorthBound|EastBound
                         };


  // ---------------------------------------------------------------
  // Local Variables.

  const char* invalidGCell =
    "Anabatic::GCellTopology () :\n\n"
    "    No GCell under point.\n";

  const char* mismatchGCell =
    "Anabatic::GCellTopology () :\n\n"
    "    Contacts under two different GCells.\n";

  const char* missingGCell =
    "Anabatic::GCellTopology () :\n\n"
    "    No Contact in GCell.\n";


  map<Component*,AutoSegment*> __routingPadAutoSegments;


  // ---------------------------------------------------------------
  // LoadGlobalRouting Local Classes.


  struct NetCompareByName {
      inline bool operator() ( const Net* lhs, const Net* rhs ) const;
  };

  inline bool NetCompareByName::operator() ( const Net* lhs, const Net* rhs ) const
  { return lhs->getName() < rhs->getName(); }


  // ---------------------------------------------------------------
  // LoadGlobalRouting Local Functions.


  void  lookupClear ()
  { __routingPadAutoSegments.clear (); }


  void  getPositions ( Component* anchor, Point& source, Point& target )
  {
    Segment* segment = dynamic_cast<Segment*>( anchor );
    if (segment) {
      source = segment->getSourcePosition();
      target = segment->getTargetPosition();
      return;
    }

    RoutingPad* rp = dynamic_cast<RoutingPad*>( anchor );
    if (rp) {
      source = rp->getSourcePosition();
      target = rp->getTargetPosition();
      return;
    }

    source = anchor->getPosition();
    target = anchor->getPosition();
  }


  uint64_t  checkRoutingPadSize ( Component* anchor )
  {
    Point  source;
    Point  target;

    size_t anchorDepth = Session::getLayerDepth( anchor->getLayer() );
    if (anchorDepth == 0) ++anchorDepth;

    getPositions( anchor, source, target );

    DbU::Unit width  = abs( target.getX() - source.getX() );
    DbU::Unit height = abs( target.getY() - source.getY() );

    uint64_t flags = 0;
    flags |= (width  < 3*Session::getPitch(anchorDepth)) ? HSmall   : 0;
    flags |= (height < 3*Session::getPitch(anchorDepth)) ? VSmall   : 0;
    flags |= ((width == 0) && (height == 0))             ? Punctual : 0;

    cdebug_log(145,0) << "::checkRoutingPadSize(): pitch[" << anchorDepth << "]:"
               << DbU::getValueString(Session::getPitch(anchorDepth)) << " "
               << ((flags & HSmall) ? "HSmall " : " ")
               << ((flags & VSmall) ? "VSmall " : " ")
               << endl;

    return flags;
  }


  Hook* getSegmentOppositeHook ( Hook* hook )
  {
    Segment* segment = static_cast<Segment*>( hook->getComponent() );
    return segment->getOppositeHook( hook );
  }


  uint64_t  getSegmentHookType ( Hook* hook )
  {
    Horizontal* horizontal = dynamic_cast<Horizontal*>( hook->getComponent() );
    if (horizontal) {
      if (horizontal->getSourceX() > horizontal->getTargetX())
        cerr << Warning("Bad orientation of %s",getString(horizontal).c_str()) << endl;

      if (dynamic_cast<Segment::SourceHook*>(hook))
        return EastBound;
      return WestBound;
    } else {
      Vertical* vertical = dynamic_cast<Vertical*>( hook->getComponent() );
      if (vertical) {
        if (vertical->getSourceY() > vertical->getTargetY())
          cerr << Warning("Bad orientation of %s",getString(vertical).c_str()) << endl;

        if (dynamic_cast<Segment::SourceHook*>(hook))
          return NorthBound;
      } else {
        cerr << Warning("Unmanaged Hook %s",getString(hook).c_str()) << endl;
      }
    }
    return SouthBound;
  }


// ---------------------------------------------------------------
// Class :  "SortHookByX".

  class SortHookByX {
    public:
      inline       SortHookByX  ( uint64_t flags );
      inline bool  operator()   ( Hook* h1, Hook* h2 );
    protected:
      uint64_t  _flags;
  };


  inline  SortHookByX::SortHookByX ( uint64_t flags )
    : _flags(flags)
  { }


  inline bool  SortHookByX::operator() ( Hook* h1, Hook* h2 )
  {
    DbU::Unit x1 = 0;
    DbU::Unit x2 = 0;
    Horizontal* hh1 = dynamic_cast<Horizontal*>(h1->getComponent());
    Horizontal* hh2 = dynamic_cast<Horizontal*>(h2->getComponent());
    Vertical*   vv1 = dynamic_cast<Vertical*>  (h1->getComponent());
    Vertical*   vv2 = dynamic_cast<Vertical*>  (h2->getComponent());

    if      (hh1) x1 = min( hh1->getSource()->getX(), hh1->getTarget()->getX() );
    else if (vv1) x1 = vv1->getX();
    else          x1 = h1->getComponent()->getCenter().getX();

    if      (hh2) x2 = min( hh2->getSource()->getX(), hh2->getTarget()->getX() );
    else if (vv2) x2 = vv2->getX();
    else          x2 = h2->getComponent()->getCenter().getX();

    if (x1 == x2) return false;
    return (_flags & SortDecreasing) xor (x1 < x2);
  }


// ---------------------------------------------------------------
// Class :  "SortHookByY".

  class SortHookByY {
    public:
      inline       SortHookByY  ( uint64_t flags );
      inline bool  operator()   ( Hook* h1, Hook* h2 );
    protected:
      uint64_t  _flags;
  };


  inline  SortHookByY::SortHookByY ( uint64_t flags )
    : _flags(flags)
  { }


  inline bool  SortHookByY::operator() ( Hook* h1, Hook* h2 )
  {
    DbU::Unit y1 = 0;
    DbU::Unit y2 = 0;
    Horizontal* hh1 = dynamic_cast<Horizontal*>(h1->getComponent());
    Horizontal* hh2 = dynamic_cast<Horizontal*>(h2->getComponent());
    Vertical*   vv1 = dynamic_cast<Vertical*>  (h1->getComponent());
    Vertical*   vv2 = dynamic_cast<Vertical*>  (h2->getComponent());

    if      (vv1) y1 = min( vv1->getSource()->getY(), vv1->getTarget()->getY() );
    else if (hh1) y1 = hh1->getY();
    else          y1 = h1->getComponent()->getCenter().getX();

    if      (vv2) y2 = min( vv2->getSource()->getY(), vv2->getTarget()->getY() );
    else if (hh2) y2 = hh2->getY();
    else          y2 = h2->getComponent()->getCenter().getY();

    if (y1 == y2) return false;
    return (_flags & SortDecreasing) xor (y1 < y2);
  }

  // ---------------------------------------------------------------
  // Class :  "SortRpByX".

  class SortRpByX {
    public:
      inline       SortRpByX  ( uint64_t flags );
      inline bool  operator() ( Component* rp1, Component* rp2 );
    protected:
      uint64_t  _flags;
  };


  inline  SortRpByX::SortRpByX ( uint64_t flags )
    : _flags(flags)
  { }


  inline bool  SortRpByX::operator() ( Component* rp1, Component* rp2 )
  {
    DbU::Unit x1 = rp1->getCenter().getX();
    DbU::Unit x2 = rp2->getCenter().getX();

    if (x1 == x2) return false;
    return (_flags & SortDecreasing) xor (x1 < x2);
  }


  // ---------------------------------------------------------------
  // Class  :  "SortRpByY".

  class SortRpByY {
    public:
      inline       SortRpByY  ( uint64_t flags );
      inline bool  operator() ( Component* rp1, Component* rp2 );
    protected:
      uint64_t  _flags;
  };


  inline  SortRpByY::SortRpByY  ( uint64_t flags )
    : _flags(flags)
  { }


  inline bool  SortRpByY::operator() ( Component* rp1, Component* rp2 )
  {
    DbU::Unit y1 = rp1->getCenter().getY();
    DbU::Unit y2 = rp2->getCenter().getY();

    if (y1 == y2) return false;
    return (_flags & SortDecreasing) xor (y1 < y2);
  }


  // ---------------------------------------------------------------
  // Class  :  "ForkStack".

  class ForkStack {
    public:
      inline void         push       ( Hook* from, AutoContact* contact );
      inline void         pop        ();
      inline Hook*        getFrom    () const;
      inline AutoContact* getContact () const;
    private:
      struct Element {
                 Hook*         _from;
                 AutoContact*  _contact;
          inline               Element ( Hook* from, AutoContact* contact );
      };
    private:
      list<Element>  _stack;
  };


  inline              ForkStack::Element::Element ( Hook* from, AutoContact* contact ) : _from(from), _contact(contact) {}
  inline void         ForkStack::pop              () { if (not _stack.empty()) _stack.pop_back(); }
  inline Hook*        ForkStack::getFrom          ()  const { return _stack.empty() ? NULL : _stack.back()._from; }
  inline AutoContact* ForkStack::getContact       ()  const { return _stack.empty() ? NULL : _stack.back()._contact; }


  inline void  ForkStack::push ( Hook* from, AutoContact* contact )
  {
    cdebug_log(145,0) << "    Stacking " << from << " + " << contact << endl;
    _stack.push_back( Element(from,contact) );
  }


  // ---------------------------------------------------------------
  // Class  :  "GGellTopology".

  class GCellTopology {

    public:
      static void          init               ( unsigned int degree );
      static void          fixSegments        ();
                           GCellTopology      ( AnabaticEngine*
                                              , ForkStack&
                                              , Hook*        fromHook
                                              , AutoContact* sourceContact=NULL );
             void          construct          ();
      inline unsigned int  getStateG          () const;
      inline GCell*        getGCell           () const;
      inline Hook*         north              ( size_t i=0 ) const;
      inline Hook*         south              ( size_t i=0 ) const;
      inline Hook*         east               ( size_t i=0 ) const;
      inline Hook*         west               ( size_t i=0 ) const;
      inline bool          push               ( Hook* to, AutoContact* contact, uint64_t flags=NoFlags );
      static void          doRp_AutoContacts  ( GCell*, Component*, AutoContact*& source, AutoContact*& target, uint64_t flags );
      static AutoContact*  doRp_Access        ( GCell*, Component*, uint64_t  flags );
      static AutoContact*  doRp_AccessPad     ( RoutingPad*, uint64_t flags );
      static AutoContact*  doRp_AccessAnalog  ( GCell*, RoutingPad*, uint64_t flags );
             AutoContact*  doRp_2m_Access     ( GCell*, RoutingPad*, uint64_t flags );
      static void          doRp_StairCaseH    ( GCell*, Component* rp1, Component* rp2 );
      static void          doRp_StairCaseV    ( GCell*, Component* rp1, Component* rp2 );
    private:                                    
             void          _do_2m_1G_1M1      ();
             void          _do_2m_2G_1M1      ();
             void          _do_2m_xG          ();
             void          _do_xG             ();
             void          _do_2G             ();
             void          _do_xG_1Pad        ();
             void          _do_1G_1PinM2      ();
             void          _do_1G_1M1         ();
             void          _do_1G_xM1         ();
             void          _do_xG_xM1_xM3     ();
             void          _do_xG_1M1_1M2     ();
             void          _do_4G_1M2         ();
             void          _do_xG_xM2         ();
             void          _do_1G_1M3         ();
             void          _do_xG_xM3         ();
             AutoContact*  _doHChannel        ();
             AutoContact*  _doVChannel        ();
             AutoContact*  _doStrut           ();
             AutoContact*  _doDevice          ();
             AutoContact*  _doHRail           ();
             AutoContact*  _doVRail           ();
             void          _doIoPad           ();
      
             unsigned int  getNumberGlobals       ();
             unsigned int  getDeviceNeighbourBound();
      
      

    private:
      enum ConnexityBits { GlobalBSize = 8
                         , Metal1BSize = 4
                         , Metal2BSize = 4
                         , Metal3BSize = 4
                         , PadsBSize   = 4
                         , PinsBSize   = 4
                         };

#define CONNEXITY_VALUE( Gs, M1s, M2s, M3s, pads, pins )                 \
      Gs + ((M1s ) <<  GlobalBSize)                                      \
         + ((M2s ) << (GlobalBSize+Metal1BSize))                         \
         + ((M3s ) << (GlobalBSize+Metal1BSize+Metal2BSize))             \
         + ((pads) << (GlobalBSize+Metal1BSize+Metal2BSize+Metal3BSize)) \
         + ((pins) << (GlobalBSize+Metal1BSize+Metal2BSize+Metal3BSize+PadsBSize))

    //                     Connexity Name                    | G|M1|M2|M2|Pad|Pin|
      enum ConnexityFlag { Conn_0G          = CONNEXITY_VALUE( 0, 0, 0, 0, 0 , 0 )
                         , Conn_2G          = CONNEXITY_VALUE( 2, 0, 0, 0, 0 , 0 )
                         , Conn_3G          = CONNEXITY_VALUE( 3, 0, 0, 0, 0 , 0 )
                         , Conn_4G          = CONNEXITY_VALUE( 4, 0, 0, 0, 0 , 0 )
                         , Conn_0G_2M1      = CONNEXITY_VALUE( 0, 2, 0, 0, 0 , 0 )
                         , Conn_1G_1M1      = CONNEXITY_VALUE( 1, 1, 0, 0, 0 , 0 )
                         , Conn_1G_2M1      = CONNEXITY_VALUE( 1, 2, 0, 0, 0 , 0 )
                         , Conn_1G_3M1      = CONNEXITY_VALUE( 1, 3, 0, 0, 0 , 0 )
                         , Conn_1G_4M1      = CONNEXITY_VALUE( 1, 4, 0, 0, 0 , 0 )
                         , Conn_1G_5M1      = CONNEXITY_VALUE( 1, 5, 0, 0, 0 , 0 )
                         , Conn_1G_1M2      = CONNEXITY_VALUE( 1, 0, 1, 0, 0 , 0 )
                         , Conn_1G_2M2      = CONNEXITY_VALUE( 1, 0, 2, 0, 0 , 0 )
                         , Conn_1G_3M2      = CONNEXITY_VALUE( 1, 0, 3, 0, 0 , 0 )
                         , Conn_1G_4M2      = CONNEXITY_VALUE( 1, 0, 4, 0, 0 , 0 )
                         , Conn_1G_1M3      = CONNEXITY_VALUE( 1, 0, 0, 1, 0 , 0 )
                         , Conn_1G_2M3      = CONNEXITY_VALUE( 1, 0, 0, 2, 0 , 0 )
                         , Conn_1G_3M3      = CONNEXITY_VALUE( 1, 0, 0, 3, 0 , 0 )
                         , Conn_1G_4M3      = CONNEXITY_VALUE( 1, 0, 0, 4, 0 , 0 )
                         , Conn_1G_1M1_1M2  = CONNEXITY_VALUE( 1, 1, 1, 0, 0 , 0 )
                         , Conn_1G_1M1_1M3  = CONNEXITY_VALUE( 1, 1, 0, 1, 0 , 0 )
                         // Connexity Name                   | G|M1|M2|M2|Pad|Pin|
                         , Conn_2G_1M1      = CONNEXITY_VALUE( 2, 1, 0, 0, 0 , 0 )
                         , Conn_2G_2M1      = CONNEXITY_VALUE( 2, 2, 0, 0, 0 , 0 )
                         , Conn_2G_3M1      = CONNEXITY_VALUE( 2, 3, 0, 0, 0 , 0 )
                         , Conn_2G_4M1      = CONNEXITY_VALUE( 2, 4, 0, 0, 0 , 0 )
                         , Conn_2G_5M1      = CONNEXITY_VALUE( 2, 5, 0, 0, 0 , 0 )
                         , Conn_2G_1M2      = CONNEXITY_VALUE( 2, 0, 1, 0, 0 , 0 )
                         , Conn_2G_2M2      = CONNEXITY_VALUE( 2, 0, 2, 0, 0 , 0 )
                         , Conn_2G_3M2      = CONNEXITY_VALUE( 2, 0, 3, 0, 0 , 0 )
                         , Conn_2G_4M2      = CONNEXITY_VALUE( 2, 0, 4, 0, 0 , 0 )
                         , Conn_2G_1M3      = CONNEXITY_VALUE( 2, 0, 0, 1, 0 , 0 )
                         , Conn_2G_2M3      = CONNEXITY_VALUE( 2, 0, 0, 2, 0 , 0 )
                         , Conn_2G_3M3      = CONNEXITY_VALUE( 2, 0, 0, 3, 0 , 0 )
                         , Conn_2G_4M3      = CONNEXITY_VALUE( 2, 0, 0, 4, 0 , 0 )
                         , Conn_2G_1M1_1M2  = CONNEXITY_VALUE( 2, 1, 1, 0, 0 , 0 )
                         // Connexity Name                   | G|M1|M2|M2|Pad|Pin|
                         , Conn_3G_1M1      = CONNEXITY_VALUE( 3, 1, 0, 0, 0 , 0 )
                         , Conn_3G_2M1      = CONNEXITY_VALUE( 3, 2, 0, 0, 0 , 0 )
                         , Conn_3G_3M1      = CONNEXITY_VALUE( 3, 3, 0, 0, 0 , 0 )
                         , Conn_3G_4M1      = CONNEXITY_VALUE( 3, 4, 0, 0, 0 , 0 )
                         , Conn_3G_1M2      = CONNEXITY_VALUE( 3, 0, 1, 0, 0 , 0 )
                         , Conn_3G_2M2      = CONNEXITY_VALUE( 3, 0, 2, 0, 0 , 0 )
                         , Conn_3G_1M3      = CONNEXITY_VALUE( 3, 0, 0, 1, 0 , 0 )
                         , Conn_3G_2M3      = CONNEXITY_VALUE( 3, 0, 0, 2, 0 , 0 )
                         , Conn_3G_3M3      = CONNEXITY_VALUE( 3, 0, 0, 3, 0 , 0 )
                         , Conn_3G_4M3      = CONNEXITY_VALUE( 3, 0, 0, 4, 0 , 0 )
                         // Connexity Name                   | G|M1|M2|M2|Pad|Pin|
                         , Conn_4G_1M1      = CONNEXITY_VALUE( 4, 1, 0, 0, 0 , 0 )
                         , Conn_4G_2M1      = CONNEXITY_VALUE( 4, 2, 0, 0, 0 , 0 )
                         , Conn_4G_3M1      = CONNEXITY_VALUE( 4, 3, 0, 0, 0 , 0 )
                         , Conn_4G_4M1      = CONNEXITY_VALUE( 4, 4, 0, 0, 0 , 0 )
                         , Conn_4G_1M2      = CONNEXITY_VALUE( 4, 0, 1, 0, 0 , 0 )
                         , Conn_4G_1M3      = CONNEXITY_VALUE( 4, 0, 0, 1, 0 , 0 )
                         , Conn_1G_1Pad     = CONNEXITY_VALUE( 1, 0, 0, 0, 1 , 0 )
                         , Conn_2G_1Pad     = CONNEXITY_VALUE( 2, 0, 0, 0, 1 , 0 )
                         , Conn_3G_1Pad     = CONNEXITY_VALUE( 3, 0, 0, 0, 1 , 0 )
                         , Conn_1G_1PinM2   = CONNEXITY_VALUE( 1, 0, 1, 0, 0 , 1 )
                         , Conn_2G_1PinM2   = CONNEXITY_VALUE( 2, 0, 1, 0, 0 , 1 )
                         };

    // Connexity Union Type.
      union UConnexity {
        uint64_t connexity;
        struct {
          unsigned int globals : GlobalBSize;
          unsigned int M1      : Metal1BSize;
          unsigned int M2      : Metal2BSize;
          unsigned int M3      : Metal3BSize;
          unsigned int Pad     : PadsBSize;
          unsigned int Pin     : PinsBSize;
        } fields;
      };

      enum TopologyFlag { Global_Vertical_End   = 0x00000001
                        , Global_Horizontal_End = 0x00000002
                        , Global_Horizontal     = 0x00000004
                        , Global_Vertical       = 0x00000008
                        , Global_Turn           = 0x00000010
                        , Global_Fork           = 0x00000020
                        , Global_Fixed          = 0x00000040
                        , Global_End            = Global_Vertical_End | Global_Horizontal_End
                        , Global_Split          = Global_Horizontal | Global_Vertical | Global_Fork
                        };

    // Attributes.
    private:
      static vector<AutoSegment*>  _toFixSegments;
      static unsigned int          _degree;
             ForkStack&            _forks;
             UConnexity            _connexity;
             unsigned int          _topology;
             Net*                  _net;
             GCell*                _gcell;
             AutoContact*          _sourceContact;
             AutoContact*          _southWestContact;
             AutoContact*          _northEastContact;
             Hook*                 _fromHook;
             vector<Hook*>         _easts;
             vector<Hook*>         _wests;
             vector<Hook*>         _norths;
             vector<Hook*>         _souths;
             vector<RoutingPad*>   _routingPads;
  };


  inline unsigned int  GCellTopology::getStateG () const { return _connexity.fields.globals; }
  inline GCell*        GCellTopology::getGCell  () const { return _gcell; }
  inline Hook*         GCellTopology::north     ( size_t i ) const { return (i<_norths.size()) ? _norths[i] : NULL; }
  inline Hook*         GCellTopology::south     ( size_t i ) const { return (i<_souths.size()) ? _souths[i] : NULL; }
  inline Hook*         GCellTopology::east      ( size_t i ) const { return (i<_easts .size()) ? _easts [i] : NULL; }
  inline Hook*         GCellTopology::west      ( size_t i ) const { return (i<_wests .size()) ? _wests [i] : NULL; }

  inline bool  GCellTopology::push ( Hook* toHook, AutoContact* contact, uint64_t flags )
  {
    cdebug_log(145,0) << "GCellTopology::push()" << endl;
    cdebug_log(145,0) << "* toHook:   " << toHook << endl;
    cdebug_log(145,0) << "* _fromHook:" << _fromHook << endl;

    if (not toHook or (toHook == _fromHook)) {
      if (contact) {
        if ( (flags & SouthWest) and not _southWestContact ) {
          cdebug_log(145,0) << "Setting _southWestContact:" << contact << endl;
          _southWestContact = contact;
        }
        if ( (flags & NorthEast) and not _northEastContact ) {
          cdebug_log(145,0) << "Setting _northEastContact:" << contact << endl;
          _northEastContact = contact;
        }
      }
      return false;
    }

    Hook* toHookOpposite = getSegmentOppositeHook( toHook );
    cdebug_log(145,0) << "Pushing (to)   " << getString(toHook) << endl;
    cdebug_log(145,0) << "Pushing (from) " << contact << endl;
    _forks.push( toHookOpposite, contact );

    return true;
  }


  vector<AutoSegment*>  GCellTopology::_toFixSegments;
  unsigned int          GCellTopology::_degree         = 0;


  void  GCellTopology::fixSegments ()
  {
    for ( size_t i=0 ; i<_toFixSegments.size() ; ++i )
      _toFixSegments[i]->setFlags( AutoSegment::SegFixed );
    _toFixSegments.clear();
  }


  void  GCellTopology::init ( unsigned int degree )
  {
    _degree = degree;
    _toFixSegments.clear();
  }


  GCellTopology::GCellTopology ( AnabaticEngine* anbt
                               , ForkStack&      forks
                               , Hook*           fromHook
                               , AutoContact*    sourceContact )
    : _forks           (forks)
    , _connexity       ()
    , _topology        (0)
    , _gcell           (NULL)
    , _sourceContact   (sourceContact)
    , _southWestContact(NULL)
    , _northEastContact(NULL)
    , _fromHook        (fromHook)
    , _easts           ()
    , _wests           ()
    , _norths          ()
    , _souths          ()
    , _routingPads     ()
  {
    _connexity.connexity = 0;

    cdebug_log(145,1) << "GCellTopology::GCellTopology()" << endl;
    cdebug_log(145,0) << "* _fromHook:     " << fromHook << endl;
    cdebug_log(145,0) << "* _sourceContact:" << sourceContact << endl;

    Segment* fromSegment = static_cast<Segment*>( _fromHook->getComponent() );
    _net = fromSegment->getNet();

    cdebug_log(145,0) << "For Hooks from fromHook" << endl;
    for ( Hook* hook : fromHook->getHooks() ) {
      cdebug_log(145,0) << "Hook: " << hook << endl;
      cdebug_log(145,0) << "Topology [" << _connexity.connexity << "] = "
                 << "["  << (int)_connexity.fields.globals
                 << "+"  << (int)_connexity.fields.M1     
                 << "+"  << (int)_connexity.fields.M2     
                 << "+"  << (int)_connexity.fields.M3
                 << "+"  << (int)_connexity.fields.Pin
                 << "+"  << (int)_connexity.fields.Pad
                 << "] " << _gcell
                 << endl;

      Segment* toSegment = dynamic_cast<Segment*>( hook->getComponent() );
      if (toSegment) {
        switch ( getSegmentHookType(hook) ) {
          case WestBound:  _wests .push_back( hook ); break;
          case EastBound:  _easts .push_back( hook ); break;
          case SouthBound: _souths.push_back( hook ); break;
          case NorthBound: _norths.push_back( hook ); break;
        }

        _connexity.fields.globals++;
      } else {
        Component*  anchor = hook->getComponent();
        RoutingPad* rp     = dynamic_cast<RoutingPad*>( anchor );

        cdebug_log(145,0) << "| Looking for Anchor:" << anchor << endl;

        if (anchor) {
          Contact* contact = dynamic_cast<Contact*>( anchor );
          cdebug_log(145,0) << "Contact:" << contact << endl;
          if (contact
             and (  Session::getAnabatic()->getConfiguration()->isGContact( anchor->getLayer() )
                 or Session::getAnabatic()->getConfiguration()->isGMetal  ( anchor->getLayer() )) ) {
          // Global routing articulation contact are in <gmetalh> not <gcut> ?
            GCell* gcell = anbt->getGCellUnder( contact->getCenter() );
            cdebug_log(145,0) << "* Global Routing Articulation: " << contact << endl;
            cdebug_log(145,0) << "| " << gcell << endl;
            if (gcell == NULL)
              throw Error( invalidGCell );
            if (_gcell == NULL) _gcell = gcell;
            else if (_gcell != gcell) {
              throw Error( mismatchGCell );
            }

            if (not _gcell->isMatrix()) {
              cdebug_log(145,0) << "* Non-matrix GCell under: " << contact << endl;
              cdebug_log(145,0) << "| " << gcell << endl;
            }
          } else {
            if (rp and AllianceFramework::get()->isPad(rp->_getEntityAsComponent()->getCell())) {
              _connexity.fields.Pad++;
            } else {
              const Layer* layer = anchor->getLayer();
              cdebug_log(145,0) << "rp: " << rp << endl;

              if      (layer->getMask() == Session::getRoutingLayer(0)->getMask()) _connexity.fields.M1++; // M1 V
              else if (layer->getMask() == Session::getRoutingLayer(1)->getMask()) _connexity.fields.M2++; // M2 H
              else if (layer->getMask() == Session::getRoutingLayer(2)->getMask()) _connexity.fields.M3++; // M3 V
              else if (layer->getMask() == Session::getRoutingLayer(3)->getMask()) _connexity.fields.M2++; // M4 H
              else if (layer->getMask() == Session::getRoutingLayer(4)->getMask()) _connexity.fields.M3++; // M5 V
              else {
                cerr << Warning( "Terminal layer \"%s\" of %s is not managed yet (ignored)."
                               , getString(layer->getName()).c_str()
                               , getString(anchor).c_str() )
                     << endl;
              //continue;
              }

              if (dynamic_cast<Pin*>(rp->getOccurrence().getEntity())) _connexity.fields.Pin++; 
            }

            cdebug_log(145,0) << "| Component to connect: " << anchor << endl;
            _routingPads.push_back( rp );
          }
        }
      }
    }

    if (_gcell->isMatrix()) {
      cdebug_log(145,0) << "east: " << east () << endl;
      cdebug_log(145,0) << "west: " << west () << endl;
      cdebug_log(145,0) << "north:" << north() << endl;
      cdebug_log(145,0) << "south:" << south() << endl;

      if (_connexity.fields.globals == 1) {
        if ( north() or south() ) _topology |= Global_Vertical_End;
        else                      _topology |= Global_Horizontal_End;
      } else if (_connexity.fields.globals == 2) {
        if      ( east () and west () ) _topology |= Global_Horizontal;
        else if ( north() and south() ) _topology |= Global_Vertical;
        else                            _topology |= Global_Turn;
      } else {
        _topology |= Global_Fork;
      }
    }

    cdebug_tabw(145,-1);

    if (_gcell == NULL) throw Error( missingGCell );
  }


  void  GCellTopology::construct ()
  {
    cdebug_log(145,1) << "GCellTopology::construct() [" << _connexity.connexity << "] in " << _gcell << endl;

    _southWestContact = NULL;
    _northEastContact = NULL;

    if (not _gcell->isAnalog()) {
      if (_gcell->isMatrix()) {
        switch ( _connexity.connexity ) {
          case Conn_1G_1Pad:
          case Conn_2G_1Pad:
          case Conn_3G_1Pad:   _do_xG_1Pad  (); break;
          case Conn_1G_1PinM2: _do_1G_1PinM2(); break;
          case Conn_1G_1M1:    _do_1G_1M1   (); break;
          case Conn_1G_2M1:
          case Conn_1G_3M1:
          case Conn_1G_4M1:
          case Conn_1G_5M1:  _do_1G_xM1(); break;
          case Conn_1G_1M2:
          case Conn_1G_2M2:
          case Conn_1G_3M2:
          case Conn_1G_4M2:  _do_xG_xM2(); break;
          case Conn_1G_1M3:  _do_1G_1M3(); break;
          case Conn_1G_2M3:
          case Conn_1G_3M3:
          case Conn_1G_4M3:     _do_xG_xM3    (); break;
          case Conn_1G_1M1_1M2: _do_xG_1M1_1M2(); break;
          case Conn_1G_1M1_1M3: _do_1G_xM1    (); break;
          case Conn_2G_1M1:
          case Conn_2G_2M1:
          case Conn_2G_3M1:
          case Conn_2G_4M1:
          case Conn_2G_5M1:
          case Conn_3G_1M1:
          case Conn_3G_2M1:
          case Conn_3G_3M1:
          case Conn_3G_4M1:
          case Conn_3G_2M3:
          case Conn_3G_3M3:
          case Conn_3G_4M3:
          case Conn_4G_1M1:
          case Conn_4G_2M1:
          case Conn_4G_3M1:
          case Conn_4G_4M1: _do_xG_xM1_xM3(); break;
          case Conn_4G_1M2: _do_4G_1M2    (); break;
          case Conn_2G_1M2:
          case Conn_2G_2M2:
          case Conn_2G_3M2:
          case Conn_2G_4M2:
          case Conn_3G_1M2:
          case Conn_3G_2M2: _do_xG_xM2(); break;
          case Conn_2G_1M3:
          case Conn_2G_2M3:
          case Conn_2G_3M3:
          case Conn_2G_4M3:
          case Conn_3G_1M3:     _do_xG_xM3    (); break;
          case Conn_2G_1M1_1M2: _do_xG_1M1_1M2(); break;
          case Conn_2G_1PinM2:  _do_xG_xM2    (); break;
          case Conn_2G:         _do_2G        (); break;
          case Conn_3G:
          case Conn_4G:
            _do_xG();
            break;
          default:
            throw Bug( "Unmanaged Configuration [%d] = [%d+%d+%d+%d,%d+%d] %s in %s\n"
                       "      The global routing seems to be defective."
                     , _connexity.connexity
                     , _connexity.fields.globals
                     , _connexity.fields.M1     
                     , _connexity.fields.M2     
                     , _connexity.fields.M3
                     , _connexity.fields.Pin
                     , _connexity.fields.Pad
                     , _net->_getString().c_str()
                     , getString(_gcell).c_str()
                     );
            _do_xG();
        }
  
        if (_sourceContact) {
          AutoContact* targetContact
            = ( getSegmentHookType(_fromHook) & (NorthBound|EastBound) )
              ? _northEastContact : _southWestContact ;
          AutoSegment* globalSegment = AutoSegment::create( _sourceContact
                                                          , targetContact
                                                          , static_cast<Segment*>( _fromHook->getComponent() )
                                                          );
          globalSegment->setFlags( (_degree == 2) ? AutoSegment::SegBipoint : 0 );
          cdebug_log(145,0) << "Create global segment: " << globalSegment << endl;
  
        // HARDCODED VALUE.
          if ( (_topology & Global_Fixed) and (globalSegment->getLength() < 2*Session::getSliceHeight()) )
            _toFixSegments.push_back( globalSegment );
        
          if (_connexity.fields.globals < 2) {
            cdebug_tabw(145,-1);
            return;
          }
        } else
          _fromHook = NULL;
  
        push( east (), _northEastContact );
        push( west (), _southWestContact );
        push( north(), _northEastContact );
        push( south(), _southWestContact );
      } else {
        if (not _sourceContact) _fromHook = NULL;

        switch ( _connexity.connexity ) {
          case Conn_1G_1M1: _do_2m_1G_1M1(); break;
          case Conn_2G_1M1: _do_2m_2G_1M1(); break;
          default:
            if (  _connexity.fields.M1
               or _connexity.fields.M2
               or _connexity.fields.M3
               or _connexity.fields.Pin
               or _connexity.fields.Pad )
              throw Bug( "Unmanaged Configuration [%d] = [%d+%d+%d+%d,%d+%d] %s in %s\n"
                         "      The global routing seems to be defective."
                       , _connexity.connexity
                       , _connexity.fields.globals
                       , _connexity.fields.M1     
                       , _connexity.fields.M2     
                       , _connexity.fields.M3
                       , _connexity.fields.Pin
                       , _connexity.fields.Pad
                       , _net->_getString().c_str()
                       , getString(_gcell).c_str()
                       );
            _do_2m_xG();
        }
  
        if (_sourceContact) {
          Segment*     segment       = static_cast <Segment*> ( _fromHook->getComponent() );
          AutoSegment* globalSegment = AutoSegment::create( _sourceContact, _southWestContact, segment );

          globalSegment->setFlags( (_degree == 2) ? AutoSegment::SegBipoint : 0 );
          cdebug_log(145,0) << "Create global segment: " << globalSegment << endl;
  
        // HARDCODED VALUE.
          if ( (_topology & Global_Fixed) and (globalSegment->getLength() < 2*Session::getSliceHeight()) )
            _toFixSegments.push_back( globalSegment );
        
          if (_connexity.fields.globals < 2) {
            cdebug_tabw(145,-1);
            return;
          }
        } else
          _fromHook = NULL;
        
      }
    } else {
      AutoContact* targetContact = NULL;
      if (not _sourceContact) _fromHook = NULL;

      if      (_gcell->isDevice ())  targetContact = _doDevice  ();
      else if (_gcell->isHChannel()) targetContact = _doHChannel();
      else if (_gcell->isVChannel()) targetContact = _doVChannel();
      else if (_gcell->isHRail   ()) targetContact = _doHRail();
      else if (_gcell->isVRail   ()) targetContact = _doVRail();
      else if (_gcell->isStrut  ())  targetContact = _doStrut   ();
      else if (_gcell->isIoPad  ())  _doIoPad();
      else
        throw Bug( "Unmanaged GCell type: %s in %s\n"
                   "      The global routing seems to be defective."
                 , getString(_gcell).c_str()
                 , getString(_net).c_str()
                 );

      if ( (_sourceContact) && (targetContact) ){
        AutoSegment* globalSegment = AutoSegment::create( _sourceContact
                                                        , targetContact
                                                        , static_cast<Segment*>( _fromHook->getComponent() )
                                                        );
        cdebug_log(145,0) << "[Create global segment (1)]: " << globalSegment << endl;
      }     
    }

    cdebug_log(145,0) << endl;
    cdebug_tabw(145,-1);
  }


  void  GCellTopology::doRp_AutoContacts ( GCell*        gcell
                                         , Component*    rp
                                         , AutoContact*& source
                                         , AutoContact*& target
                                         , uint64_t      flags
                                         )
  {
    cdebug_log(145,1) << "doRp_AutoContacts()" << endl;
    cdebug_log(145,0)   << rp << endl;

    source = target = NULL;

    Point        sourcePosition;
    Point        targetPosition;
    const Layer* rpLayer        = rp->getLayer();
    size_t       rpDepth        = Session::getLayerDepth( rp->getLayer() );
    Flags        direction      = Session::getDirection ( rpDepth );
    DbU::Unit    viaSide        = Session::getViaWidth  ( rpDepth );

    getPositions( rp, sourcePosition, targetPosition );

    if (sourcePosition.getX() > targetPosition.getX()) swap( sourcePosition, targetPosition );
    if (sourcePosition.getY() > targetPosition.getY()) swap( sourcePosition, targetPosition );

    GCell* sourceGCell = Session::getAnabatic()->getGCellUnder( sourcePosition );
    GCell* targetGCell = Session::getAnabatic()->getGCellUnder( targetPosition );

    if (rpDepth == 0) {
      rpLayer   = Session::getContactLayer(0);
      direction = Flags::Horizontal;
      viaSide   = Session::getViaWidth( rpDepth );
    }

  // Non-M1 terminal or punctual M1 protections.
    if ((rpDepth != 0) or (sourcePosition == targetPosition)) {
      map<Component*,AutoSegment*>::iterator irp = __routingPadAutoSegments.find( rp );
      if (irp == __routingPadAutoSegments.end()) {
        AutoContact* sourceProtect = AutoContactTerminal::create( sourceGCell
                                                                , rp
                                                                , rpLayer
                                                                , sourcePosition
                                                                , viaSide, viaSide
                                                                );
        AutoContact* targetProtect = AutoContactTerminal::create( targetGCell
                                                                , rp
                                                                , rpLayer
                                                                , targetPosition
                                                                , viaSide, viaSide
                                                                );
        sourceProtect->setFlags( CntFixed );
        targetProtect->setFlags( CntFixed );

        AutoSegment* segment = AutoSegment::create( sourceProtect, targetProtect, direction );
        segment->setFlags( AutoSegment::SegFixed );

        __routingPadAutoSegments.insert( make_pair(rp,segment) );
      }
    }

    if (sourcePosition != targetPosition) {
      if (flags & DoSourceContact)
        source = AutoContactTerminal::create( sourceGCell
                                            , rp
                                            , rpLayer
                                            , sourcePosition
                                            , viaSide, viaSide
                                            );
      if (flags & DoTargetContact)
        target = AutoContactTerminal::create( targetGCell
                                            , rp
                                            , rpLayer
                                            , targetPosition
                                            , viaSide, viaSide
                                            );
    }

    if (not source and not target) {
      source = target = AutoContactTerminal::create( gcell
                                                   , rp
                                                   , rpLayer
                                                   , rp->getCenter()
                                                   , viaSide, viaSide
                                                   );
    }

    cdebug_tabw(145,-1);
    return;
  }


  AutoContact* GCellTopology::doRp_Access ( GCell* gcell, Component* rp, uint64_t flags )
  {
    cdebug_log(145,1) << "doRp_Access() - flags:" << flags << endl;

    AutoContact* rpContactSource;
    AutoContact* rpContactTarget;

    flags |= checkRoutingPadSize( rp );

    doRp_AutoContacts( gcell, rp, rpContactSource, rpContactTarget, flags );

    if (flags & HAccess) {
      if (flags & VSmall) {
        AutoContact* subContact1 = AutoContactTurn::create( gcell, rp->getNet(), Session::getContactLayer(1) );
        AutoContact* subContact2 = AutoContactTurn::create( gcell, rp->getNet(), Session::getContactLayer(1) );
        AutoSegment::create( rpContactSource, subContact1, Flags::Horizontal );
        AutoSegment::create( subContact1,     subContact2, Flags::Vertical );
        rpContactSource = subContact2;
      }
    } else {
      if (flags & HSmall) {
        AutoContact* subContact1 = AutoContactTurn::create( gcell, rp->getNet(), Session::getContactLayer(1) );
        AutoSegment::create( rpContactSource, subContact1, Flags::Horizontal );
        rpContactSource = subContact1;
      }
    }

    cdebug_tabw(145,-1);

    return rpContactSource;
  }


  AutoContact* GCellTopology::doRp_AccessPad ( RoutingPad* rp, uint64_t flags )
  {
    cdebug_log(145,1) << "doRp_AccessPad()" << endl;
    cdebug_log(145,0) << rp << endl;

  // Hardcoded: H access is METAL2 (depth=1), V access is METAL3 (depth=2).
    size_t  accessDepth = (flags & HAccess) ? 1 : 2 ;
    size_t  padDepth    = Session::getLayerDepth(rp->getLayer());
    if (padDepth > Session::getAllowedDepth()) {
      cerr << Error( "GCellTopology::doRp_AccessPad(): Pad RoutingPad %s\n"
                     "        has a layer unreachable by the router (top layer is: %s)"
                   , getString(rp).c_str()
                   , getString(Session::getRoutingLayer(Session::getAllowedDepth())).c_str()
                   ) << endl;
      padDepth = Session::getAllowedDepth();
    }

    rp->getBodyHook()->detach();

    Point rpPosition = rp->getCenter();
    Point position   = rp->getCenter();
    Box   rpbb       = rp->getBoundingBox();
    if (  (rpbb.getWidth () > 2*Session::getWireWidth(padDepth)) 
       or (rpbb.getHeight() > 2*Session::getWireWidth(padDepth)) ) {
    //cerr << "doRp_AccessPad(): connecting to non-punctual connector (RoutingPad).\n"
    //     << "                  " << rp->getNet() << "pad:" << rp->getOccurrence().getMasterCell() << endl;

      Transformation transf = rp->getOccurrence().getPath().getTransformation();
      switch ( transf.getOrientation() ) {
        case Transformation::Orientation::ID: position.setY( rpbb.getYMin() ); break;
        case Transformation::Orientation::MY: position.setY( rpbb.getYMax() ); break;
        case Transformation::Orientation::YR:
        case Transformation::Orientation::R3: position.setX( rpbb.getXMin() ); break;
        case Transformation::Orientation::R1: position.setX( rpbb.getXMax() ); break;
        default:
          break;
      }
    }

    GCell*     gcell  = Session::getAnabatic()->getGCellUnder(position);
    Component* anchor = rp;

    if (padDepth != accessDepth) {
      if (padDepth > accessDepth) {
      // Go *down* from the pad's RoutingPad.
        --padDepth;

        Contact*   target   = NULL;
        Contact*   source   = Contact::create ( rp
                                              , Session::getContactLayer(padDepth)
                                              , position.getX() - rpPosition.getX()
                                              , position.getY() - rpPosition.getY()
                                              , Session::getViaWidth(padDepth)
                                              , Session::getViaWidth(padDepth)
                                              );
      
        for ( size_t depth = padDepth ; depth >= accessDepth ; --depth ) {
          const Layer* segmentLayer = Session::getRoutingLayer(depth);
          const Layer* targetLayer  = (depth == accessDepth) ? segmentLayer
                                                             : Session::getContactLayer(depth-1);
          DbU::Unit    targetSide   = (depth == accessDepth) ? Session::getWireWidth(depth)
                                                             : Session::getViaWidth (depth-1);

          target = Contact::create( rp->getNet()
                                  , targetLayer
                                  , position.getX()
                                  , position.getY()
                                  , targetSide
                                  , targetSide
                                  );
          if (Session::getDirection(depth) == Flags::Horizontal) {
            anchor = Horizontal::create( source
                                       , target
                                       , segmentLayer
                                       , position.getY()
                                       , Session::getWireWidth(depth)
                                       );
          } else {
            anchor = Vertical::create( source
                                     , target
                                     , segmentLayer
                                     , position.getX()
                                     , Session::getWireWidth(depth)
                                     );
          }
          cdebug_log(145,0) << "Pad strap: " << anchor << endl;
          source = target;
        }
      } else {
      // Go *up* from the pad's RoutingPad.
        Contact*   target   = NULL;
        Contact*   source   = Contact::create ( rp
                                              , Session::getContactLayer(padDepth)
                                              , 0
                                              , 0
                                              , Session::getViaWidth(padDepth)
                                              , Session::getViaWidth(padDepth)
                                              );
      
        for ( size_t depth = padDepth ; depth <= accessDepth ; ++depth ) {
          const Layer* segmentLayer = Session::getRoutingLayer(depth);
          const Layer* targetLayer  = (depth == accessDepth) ? segmentLayer
                                                             : Session::getContactLayer(depth);
          DbU::Unit    targetSide   = (depth == accessDepth) ? Session::getWireWidth(depth)
                                                             : Session::getViaWidth (depth);

          target = Contact::create( rp->getNet()
                                  , targetLayer
                                  , position.getX()
                                  , position.getY()
                                  , targetSide
                                  , targetSide
                                  );
          if (Session::getDirection(depth) == Flags::Horizontal) {
            anchor = Horizontal::create( source
                                       , target
                                       , segmentLayer
                                       , position.getY()
                                       , Session::getWireWidth(depth)
                                       );
          } else {
            anchor = Vertical::create( source
                                     , target
                                     , segmentLayer
                                     , position.getX()
                                     , Session::getWireWidth(depth)
                                     );
          }
          cdebug_log(145,0) << "Pad strap: " << anchor << endl;
          source = target;
        }
      }
    }

    AutoContact* autoSource
      = AutoContactTerminal::create ( gcell
                                    , anchor
                                    , Session::getRoutingLayer(accessDepth)
                                    , position
                                    , Session::getWireWidth(accessDepth)
                                    , Session::getWireWidth(accessDepth)
                                    );
    cdebug_tabw(145,-1);
    return autoSource;
  }


  AutoContact* GCellTopology::doRp_AccessAnalog ( GCell* gcell, RoutingPad* rp, uint64_t flags )
  {
    cdebug_log(145,1) << "doRp_AccessAnalog()" << endl;
    cdebug_log(145,0) << rp << endl;

    const Layer* rpLayer  = rp->getLayer();
    size_t       rpDepth  = Session::getLayerDepth( rpLayer );
    DbU::Unit    viaSide  = Session::getViaWidth  ( rpDepth );
    Point        position = rp->getCenter();
    Point        onGrid   = Session::getNearestGridPoint( position, gcell->getConstraintBox() );

    AutoContact* contact = AutoContactTerminal::create( gcell, rp, rpLayer, position, viaSide, viaSide );

    if (position != onGrid) {
      cerr << Bug( "GCellTopology::doRp_AccessAnalog(): RoutingPad is not under any grid point.\n"
                   "      %s\n"
                   "      Using nearest grid point: %s"
                 , getString(rp).c_str()
                 , getString(onGrid).c_str()
                 ) << endl;
      contact->forceOnGrid( onGrid );
    }

    if (rpDepth != 1) {
      cerr << Bug( "GCellTopology::doRp_AccessAnalog(): RoutingPad must be in METAL2 layer.\n"
                   "      %s"
                 , getString(rp).c_str()
                 ) << endl;
    }

    cdebug_log(145,0) << contact << endl;
    cdebug_tabw(145,-1);
    return contact;
  }


  AutoContact* GCellTopology::doRp_2m_Access ( GCell* gcell, RoutingPad* rp, uint64_t flags )
  {
    cdebug_log(145,1) << "doRp_2m_Access()" << endl;
    cdebug_log(145,0) << rp << endl;

    Point        sourcePosition;
    Point        targetPosition;
    const Layer* rpLayer        = rp->getLayer();
    const Layer* viaLayer       = Session::getDContactLayer();
    DbU::Unit    viaSide        = Session::getDContactWidth();
    DbU::Unit    ypitch         = Session::getDVerticalPitch();

    getPositions( rp, sourcePosition, targetPosition );

    if (sourcePosition.getX() > targetPosition.getX()) swap( sourcePosition, targetPosition );
    if (sourcePosition.getY() > targetPosition.getY()) swap( sourcePosition, targetPosition );

    Point position = rp->getCenter();
    if (not (flags & Middle)) {
      if (flags & NorthBound) position = targetPosition;
      if (flags & SouthBound) position = sourcePosition;
    }

    DbU::Unit ycontact = (flags & SouthBound) ? gcell->getYMin() : gcell->getYMax()-ypitch;

    AutoContact* rpContact = AutoContactTerminal::create( gcell, rp, rpLayer, position, viaSide, viaSide );
    AutoContact* contact1  = AutoContactTurn::create( gcell, _net, viaLayer );
    AutoContact* contact2  = AutoContactTurn::create( gcell, _net, viaLayer );
    contact1->setPosition( position.getX(), ycontact );
    contact2->setPosition( position.getX(), ycontact );
    rpContact->setFlags( CntFixed );
    contact1 ->setFlags( CntFixed );
    contact2 ->setFlags( CntFixed );

    AutoSegment* fixed  = AutoSegment::create( rpContact, contact1, Flags::Vertical   );
    AutoSegment* dogleg = AutoSegment::create( contact1 , contact2, Flags::Horizontal );
    fixed ->setFlags( AutoSegment::SegFixed );
    dogleg->setFlags( AutoSegment::SegFixed );

    cdebug_tabw(145,-1);
    return contact2;
  }


  void  GCellTopology::doRp_StairCaseH ( GCell* gcell, Component* rp1, Component* rp2 )
  {
    cdebug_log(145,0) << "doRp_StairCaseH()" << endl;

    if (rp1->getCenter().getX() > rp2->getCenter().getX()) swap( rp1, rp2 );

    AutoContact* rp1ContactSource = NULL;
    AutoContact* rp1ContactTarget = NULL;
    AutoContact* rp2ContactSource = NULL;
    AutoContact* rp2ContactTarget = NULL;
    const Layer* viaLayer         = NULL;

    doRp_AutoContacts( gcell, rp1, rp1ContactSource, rp1ContactTarget, DoTargetContact );
    doRp_AutoContacts( gcell, rp2, rp2ContactSource, rp2ContactTarget, DoSourceContact );

    if (rp1ContactTarget->getY() == rp2ContactSource->getY()) {
      cdebug_log(145,0) << "Aligned horizontal routing pads : straight wire" << endl;

      viaLayer = rp1->getLayer();
      AutoSegment::create( rp1ContactTarget, rp2ContactSource, Flags::Horizontal );
      return;
    }

    viaLayer = Session::getContactLayer(1);

    AutoContact* subContact1 = AutoContactTurn::create( gcell, rp1->getNet(), viaLayer );
    AutoContact* subContact2 = AutoContactTurn::create( gcell, rp1->getNet(), viaLayer );

    AutoSegment::create( rp1ContactTarget, subContact1     , Flags::Horizontal );
    AutoSegment::create( subContact1     , subContact2     , Flags::Vertical   );
    AutoSegment::create( subContact1     , rp2ContactSource, Flags::Horizontal );
  }


  void  GCellTopology::doRp_StairCaseV ( GCell* gcell, Component* rp1, Component* rp2 )
  {
    cdebug_log(145,0) << "doRp_StairCaseV()" << endl;

    if (rp1->getCenter().getY() > rp2->getCenter().getY()) swap( rp1, rp2 );

    AutoContact* rp1ContactSource = NULL;
    AutoContact* rp1ContactTarget = NULL;
    AutoContact* rp2ContactSource = NULL;
    AutoContact* rp2ContactTarget = NULL;
    const Layer* viaLayer         = NULL;

    doRp_AutoContacts( gcell, rp1, rp1ContactSource, rp1ContactTarget, DoTargetContact );
    doRp_AutoContacts( gcell, rp2, rp2ContactSource, rp2ContactTarget, DoSourceContact );

    if (rp1ContactTarget->getX() == rp2ContactSource->getX()) {
      cdebug_log(145,0) << "Aligned vertical routing pads : straight wire" << endl;

      viaLayer = rp1->getLayer();
      AutoSegment::create( rp1ContactTarget, rp2ContactSource, Flags::Vertical );
      return;
    }

    viaLayer = Session::getContactLayer(1);

    AutoContact* subContact1 = AutoContactTurn::create( gcell, rp1->getNet(), viaLayer );
    AutoContact* subContact2 = AutoContactTurn::create( gcell, rp1->getNet(), viaLayer );

    AutoSegment::create( rp1ContactTarget, subContact1     , Flags::Vertical   );
    AutoSegment::create( subContact1     , subContact2     , Flags::Horizontal );
    AutoSegment::create( subContact1     , rp2ContactSource, Flags::Vertical   );
  }


  void  GCellTopology::_do_2m_1G_1M1 ()
  {
    cdebug_log(145,1) << "_do_2m_1G_1M1()" << endl;

    uint64_t  flags = NoFlags;
    if      (north()) flags |= NorthBound;
    else if (south()) flags |= SouthBound;

    AutoContact* contact = NULL;
    contact = _northEastContact = doRp_2m_Access( _gcell, _routingPads[0], flags );
    push( north(), contact, SouthWest );
    push( south(), contact, SouthWest );

    cdebug_tabw(145,-1);
  }


  void  GCellTopology::_do_2m_2G_1M1 ()
  {
    cdebug_log(145,1) << "_do_2m_2G_1M1()" << endl;

    AutoContact* contact = NULL;
    contact = doRp_2m_Access( _gcell, _routingPads[0], SouthBound|NorthBound );
    push( north(), contact, SouthWest|Middle );

    contact = doRp_2m_Access( _gcell, _routingPads[0], SouthBound|NorthBound );
    push( south(), contact, SouthWest|Middle );
    
    cdebug_tabw(145,-1);
  }


  void  GCellTopology::_do_2m_xG ()
  {
    cdebug_log(145,1) << "_do_2m_xG()" << endl;

    vector<Hook*> hooksNS = _norths;
    hooksNS.insert( hooksNS.end(), _souths.begin(), _souths.end() );
    sort( hooksNS.begin(), hooksNS.end(), SortHookByX(NoFlags) );

    const Layer* viaLayer = Session::getDContactLayer();
    AutoContact* contactW = NULL;
    AutoContact* contactE = NULL;

  // Simple turn.
    if (  (west() and not east() and (hooksNS.size() == 1))
       or (east() and not west() and (hooksNS.size() == 1)) ) {
      contactW = AutoContactTurn::create( _gcell, _net, viaLayer );
      push( west()    , contactW, SouthWest );
      push( east()    , contactW, SouthWest );
      push( hooksNS[0], contactW, SouthWest );
      cdebug_tabw(145,-1);
      return;
    }

  // Simple HTee.
    if (west() and east() and (hooksNS.size() == 1)) {
      contactW = AutoContactHTee::create( _gcell, _net, viaLayer );
      push( west()    , contactW, SouthWest );
      push( east()    , contactW, SouthWest );
      push( hooksNS[0], contactW, SouthWest );
      cdebug_tabw(145,-1);
      return;
    }
    
    cdebug_log(145,0) << "West side processing." << endl;
  // West side processing.
    if (west()) {
      contactW =  AutoContactHTee::create( _gcell, _net, viaLayer );
      push( west()    , contactW, SouthWest );
      push( hooksNS[0], contactW, SouthWest );
    } else {
      contactW =  AutoContactTurn::create( _gcell, _net, viaLayer );
      push( hooksNS[0], contactW, SouthWest );
    }

    cdebug_log(145,0) << "Middle processing." << endl;
  // Middle (North & South) processing.
    if (hooksNS.size() > 2) {
      for ( size_t i=1 ; i<hooksNS.size()-1 ; ++i ) {
        AutoContact* current = AutoContactHTee::create( _gcell, _net, viaLayer );
        AutoSegment::create( contactW, current, Flags::Horizontal );
        push( hooksNS[i], current, SouthWest );

        contactW = current;
      }
    }

    cdebug_log(145,0) << "East side processing." << endl;
  // East side processing.
    if (east()) {
      contactE =  AutoContactHTee::create( _gcell, _net, viaLayer );
      push( east(), contactE, SouthWest );
      if (hooksNS.size() > 1)
        push( hooksNS[hooksNS.size()-1], contactE, SouthWest );
    } else {
      contactE =  AutoContactTurn::create( _gcell, _net, viaLayer );
      push( hooksNS[hooksNS.size()-1], contactE, SouthWest );
    }

    AutoSegment::create( contactW, contactE, Flags::Horizontal );

    cdebug_tabw(145,-1);
  }


  void  GCellTopology::_do_xG ()
  {
    cdebug_log(145,1) << "_do_xG()" << endl;

    const Layer* viaLayer = Session::getDContactLayer();

    if (_connexity.fields.globals == 2) {
      _southWestContact = _northEastContact = AutoContactTurn::create( _gcell, _net, viaLayer );
    } else if (_connexity.fields.globals == 3) {
      if (east() and west()) {
        _southWestContact = AutoContactTurn::create( _gcell, _net, viaLayer );
        _northEastContact = AutoContactVTee::create( _gcell, _net, viaLayer );
        if (south()) swap( _southWestContact, _northEastContact );

        AutoSegment::create( _southWestContact, _northEastContact, Flags::Vertical );
      } else {
        _southWestContact = AutoContactTurn::create( _gcell, _net, viaLayer );
        _northEastContact = AutoContactHTee::create( _gcell, _net, viaLayer );
        if (west()) swap( _southWestContact, _northEastContact );

        AutoSegment::create( _southWestContact, _northEastContact, Flags::Horizontal );
      }
    } else { // fields.globals == 4.
      AutoContact* turn = AutoContactTurn::create( _gcell, _net, viaLayer );
      _southWestContact = AutoContactHTee::create( _gcell, _net, viaLayer );
      _northEastContact = AutoContactVTee::create( _gcell, _net, viaLayer );
      AutoSegment::create( _southWestContact, turn, Flags::Horizontal );
      AutoSegment::create( turn, _northEastContact, Flags::Vertical   );
    } 
    cdebug_tabw(145,-1);
  }


  void  GCellTopology::_do_2G ()
  {
    cdebug_log(145,1) << "_do_2G()" << endl;

    const Layer* viaLayer = Session::getDContactLayer();

    if (east() and west()) {
      _southWestContact = AutoContactTurn::create( _gcell, _net, viaLayer );
      _northEastContact = AutoContactTurn::create( _gcell, _net, viaLayer );
      AutoSegment::create( _southWestContact, _northEastContact, Flags::Vertical );
    } else if (south() and north()) {
      _southWestContact = AutoContactTurn::create( _gcell, _net, viaLayer );
      _northEastContact = AutoContactTurn::create( _gcell, _net, viaLayer );
      AutoSegment::create( _southWestContact, _northEastContact, Flags::Horizontal );
    } else {
      _southWestContact = _northEastContact = AutoContactTurn::create( _gcell, _net, viaLayer );
    }

    cdebug_tabw(145,-1);
  }


  void  GCellTopology::_do_xG_1Pad ()
  {
    cdebug_log(145,1) << "_do_xG_1Pad() [Managed Configuration - Optimized] " << _topology << endl;
    cdebug_log(145,0)   << "_connexity.globals:" << (int)_connexity.fields.globals << endl;

    uint64_t  flags       = NoFlags;
    bool      eastPad     = false;
    bool      westPad     = false;
    bool      northPad    = false;
    bool      southPad    = false;
    Instance* padInstance = _routingPads[0]->getOccurrence().getPath().getHeadInstance();

    switch ( padInstance->getTransformation().getOrientation() ) {
      case Transformation::Orientation::ID: northPad = true; break;
      case Transformation::Orientation::MY: southPad = true; break;
      case Transformation::Orientation::YR:
      case Transformation::Orientation::R3: eastPad  = true; flags |= HAccess; break;
      case Transformation::Orientation::R1: westPad  = true; flags |= HAccess; break;
      default:
        cerr << Warning( "Unmanaged orientation %s for pad <%s>."
                       , getString(padInstance->getTransformation().getOrientation()).c_str()
                       , getString(padInstance).c_str() ) << endl;
        break;
    }
    cdebug_log(145,0) << "eastPad:"  << eastPad  << ", "
                      << "westPad:"  << westPad  << ", "
                      << "northPad:" << northPad << ", "
                      << "southPad:" << southPad
                      << endl;

    AutoContact* source = doRp_AccessPad( _routingPads[0], flags );
    // Point        position     = _routingPads[0]->getCenter();
    // AutoContact* source       = NULL;
    // GCell*       gcell        = Session::getAnabatic()->getGCellGrid()->getGCell(position);

    // source = AutoContactTerminal::create ( gcell
    //                                      , _routingPads[0]
    //                                      , Session::getContactLayer(3)
    //                                      , position
    //                                      , Session::getViaWidth(3), Session::getViaWidth(3)
    //                                      );
    // source->setFlags( CntFixed );

    // if (northPad or eastPad) {
    //   _southWestContact = _northEastContact = source;
    //   cdebug_tabw(145,-1);
    //   return;
    // }

  // Check for straight lines, which are not managed by _do_xG().
    if (_connexity.fields.globals == 1) {
      if (  (westPad and (east() != NULL))
         or (eastPad and (west() != NULL)) ) {
        AutoContact* turn = AutoContactTurn::create( _gcell, _net, Session::getContactLayer(1) );
        _northEastContact = _southWestContact
                          = AutoContactTurn::create( _gcell, _net, Session::getContactLayer(1) );
        AutoSegment::create( source, turn, Flags::Horizontal );
        AutoSegment::create( turn, _northEastContact, Flags::Vertical );
        cdebug_tabw(145,-1);
        return;
      } else if (  (southPad and (north() != NULL))
                or (northPad and (south() != NULL)) ) {
        AutoContact* turn = AutoContactTurn::create( _gcell, _net, Session::getContactLayer(1) );
        _northEastContact = _southWestContact
                          = AutoContactTurn::create( _gcell, _net, Session::getContactLayer(1) );
        AutoSegment::create( source, turn, Flags::Vertical );
        AutoSegment::create( turn, _northEastContact, Flags::Horizontal );
        cdebug_tabw(145,-1);
        return;
      }
    }

    ++_connexity.fields.globals;
    --_connexity.fields.Pad;

    if (westPad ) _wests .push_back( source->getBodyHook() );
    if (eastPad ) _easts .push_back( source->getBodyHook() );
    if (southPad) _souths.push_back( source->getBodyHook() );
    if (northPad) _norths.push_back( source->getBodyHook() );

    _do_xG();

    if (westPad) {
      AutoSegment::create( source, _southWestContact, Flags::Horizontal );
      _wests.clear();
    }
    if (eastPad) {
      AutoSegment::create( source, _northEastContact, Flags::Horizontal );
      _easts.clear();
    }
    if (southPad) {
      AutoSegment::create( source, _southWestContact, Flags::Vertical );
      _souths.clear();
    }
    if (northPad) {
      AutoSegment::create( source, _northEastContact, Flags::Vertical );
      _norths.clear();
    }
    --_connexity.fields.globals;

    cdebug_tabw(145,-1);
  }


  void  GCellTopology::_do_1G_1PinM2 ()
  {
    cdebug_log(145,1) << "_do_1G_1PinM2() [Managed Configuration - Optimized] " << _topology << endl;

    AutoContact* rpContact = doRp_Access( _gcell, _routingPads[0], NoFlags );
    AutoContact* turn1     = AutoContactTurn::create( _gcell, _net, Session::getContactLayer(1) );
    AutoSegment::create( rpContact, turn1, Flags::Vertical );

    if (north() or south()) {
      AutoContact* turn2 = AutoContactTurn::create( _gcell, _net, Session::getContactLayer(1) );
      AutoSegment::create( turn1, turn2, Flags::Horizontal );
      turn1 = turn2;
    }
    _southWestContact = _northEastContact = turn1;

    cdebug_tabw(145,-1);
  }


  void  GCellTopology::_do_1G_1M1 ()
  {
    cdebug_log(145,1) << "_do_1G_1M1() [Managed Configuration - Optimized] " << _topology << endl;

    uint64_t  flags = NoFlags;
    if      (east() ) { flags |= HAccess; }
    else if (west() ) { flags |= HAccess; }
    else if (north()) { flags |= VSmall; }
    else if (south()) { flags |= VSmall; }

    _southWestContact = _northEastContact = doRp_Access( _gcell, _routingPads[0], flags );

    cdebug_tabw(145,-1);
  }


  void  GCellTopology::_do_1G_xM1 ()
  {
    cdebug_log(145,1) << "_do_1G_" << (int)_connexity.fields.M1 << "M1() [Managed Configuration]" << endl;

    sort( _routingPads.begin(), _routingPads.end(), SortRpByX(NoFlags) ); // increasing X.
    for ( size_t i=1 ; i<_routingPads.size() ; ++i ) {
      AutoContact* leftContact  = doRp_Access( _gcell, _routingPads[i-1], HAccess );
      AutoContact* rightContact = doRp_Access( _gcell, _routingPads[i  ], HAccess );
      AutoSegment::create( leftContact, rightContact, Flags::Horizontal );
    }

    Component* globalRp = NULL;
    if      (east()) globalRp = _routingPads[_routingPads.size()-1];
    else if (west()) globalRp = _routingPads[0];
    else {
      globalRp = _routingPads[0];

      cdebug_log(145,0) << "| Initial N/S Global RP: " << globalRp << endl;
      for ( size_t i=1 ; i<_routingPads.size() ; ++i ) {
        if (_routingPads[i]->getBoundingBox().getHeight() > globalRp->getBoundingBox().getHeight()) {
          cdebug_log(145,0) << "| Better RP: " << globalRp << endl;
          globalRp = _routingPads[i];
        }
      }
    }
    
    AutoContact* globalContact = doRp_Access( _gcell, globalRp, HAccess );

    if (north() or south()) {
      AutoContact* turn = globalContact;
      globalContact = AutoContactTurn::create( _gcell, _net, Session::getContactLayer(1) );
      AutoSegment::create( globalContact, turn, Flags::Horizontal );
    }

    _northEastContact = _southWestContact = globalContact;

    cdebug_tabw(145,-1);
  }


  void  GCellTopology::_do_xG_1M1_1M2 ()
  {
    cdebug_log(145,1) << "_do_xG_1M1_1M2() [Managed Configuration]" << endl;

    Component* rpL1;
    Component* rpL2;
    if (_routingPads[0]->getLayer() == Session::getRoutingLayer(0)) {
      rpL1 = _routingPads[0];
      rpL2 = _routingPads[1];
    } else {
      rpL1 = _routingPads[1];
      rpL2 = _routingPads[0];
    }
    cdebug_log(145,0) << "rpL1 := " << rpL1 << endl;
    cdebug_log(145,0) << "rpL2 := " << rpL2 << endl;

    AutoContact* rpL1ContactSource = NULL;
    AutoContact* rpL1ContactTarget = NULL;
    AutoContact* rpL2ContactSource = NULL;
    AutoContact* rpL2ContactTarget = NULL;

    doRp_AutoContacts( _gcell, rpL1, rpL1ContactSource, rpL1ContactTarget, NoFlags );
    doRp_AutoContacts( _gcell, rpL2, rpL2ContactSource, rpL2ContactTarget, NoFlags );

    const Layer* viaLayer1 = Session::getContactLayer(1);
    const Layer* viaLayer2 = Session::getContactLayer(2);

    AutoContact* subContact = AutoContactTurn::create( _gcell, _net, viaLayer1 );
    AutoSegment::create( rpL1ContactSource, subContact, Flags::Horizontal );
    AutoSegment::create( rpL2ContactSource, subContact, Flags::Vertical );

    if (south() or west()) {
      doRp_AutoContacts( _gcell, rpL2, rpL2ContactSource, rpL2ContactTarget, DoSourceContact );
      if (south() and west()) {
        _southWestContact = AutoContactHTee::create( _gcell, _net, viaLayer2 );
        AutoSegment::create( rpL2ContactSource, _southWestContact, Flags::Horizontal );
      } else {
        if (south()) {
          _southWestContact = AutoContactTurn::create( _gcell, _net, viaLayer2 );
          AutoSegment::create( rpL2ContactSource, _southWestContact, Flags::Horizontal );
        } else {
          _southWestContact = rpL2ContactSource;
        }
      }
    }

    if (north() or east()) {
      doRp_AutoContacts( _gcell, rpL2, rpL2ContactSource, rpL2ContactTarget, DoTargetContact );
      if (north() and east()) {
        _northEastContact = AutoContactHTee::create( _gcell, _net, viaLayer2 );
        AutoSegment::create( rpL2ContactTarget, _northEastContact, Flags::Horizontal );
      } else {
        if (north()) {
          _northEastContact = AutoContactTurn::create( _gcell, _net, viaLayer2 );
          AutoSegment::create( rpL2ContactTarget, _northEastContact, Flags::Horizontal );
        } else {
          _northEastContact = rpL2ContactTarget;
        }
      }
    }

    cdebug_tabw(145,-1);
  }


  void  GCellTopology::_do_xG_xM1_xM3 ()
  {
    cdebug_log(145,1) << "_do_xG_"  << (int)_connexity.fields.M1
                      << "M1_"      << (int)_connexity.fields.M3
                      << "M3() [G:" << (int)_connexity.fields.globals << " Managed Configuration]" << endl;
    cdebug_log(145,0) << "_connexity: " << _connexity.connexity << endl;
    cdebug_log(145,0) << "north:     " << north() << endl;
    cdebug_log(145,0) << "south:     " << south() << endl;
    cdebug_log(145,0) << "east:      " << east() << endl;
    cdebug_log(145,0) << "west:      " << west() << endl;

    if (Session::getAllowedDepth() < 2) {
    }

    Component* rpM3 = NULL;
    if (_routingPads[0]->getLayer() == Session::getRoutingLayer(2))
      rpM3 = _routingPads[0];

    sort( _routingPads.begin(), _routingPads.end(), SortRpByX(NoFlags) ); // increasing X.
    for ( size_t i=1 ; i<_routingPads.size() ; ++i ) {
      AutoContact* leftContact  = doRp_Access( _gcell, _routingPads[i-1], HAccess );
      AutoContact* rightContact = doRp_Access( _gcell, _routingPads[i  ], HAccess );
      AutoSegment::create( leftContact, rightContact, Flags::Horizontal );

      if (not rpM3 and (_routingPads[i]->getLayer() == Session::getRoutingLayer(2)))
        rpM3 = _routingPads[i];
    }

    const Layer* viaLayer1     = Session::getContactLayer(1);
    AutoContact* unusedContact = NULL;

    if (rpM3) {
    // At least one M3 RoutingPad is present: use it.
      if (west() and not south()) {
        _southWestContact = doRp_Access( _gcell, _routingPads[0], HAccess );
      } else if (not west() and south()) {
        doRp_AutoContacts( _gcell, rpM3, _southWestContact, unusedContact, DoSourceContact );
      } else if (west() and south()) {
        AutoContact* rpContact = NULL;
        doRp_AutoContacts( _gcell, rpM3, rpContact, unusedContact, DoSourceContact );
        _southWestContact = AutoContactVTee::create( _gcell, _net, viaLayer1 );
        AutoSegment::create( rpContact, _southWestContact, Flags::Vertical );
      }

      if (east() and not north()) {
        _northEastContact = doRp_Access( _gcell, _routingPads[_routingPads.size()-1], HAccess );
      } else if (not east() and north()) {
        doRp_AutoContacts( _gcell, rpM3, unusedContact, _northEastContact, DoTargetContact );
      } else if (east() and north()) {
        AutoContact* rpContact = NULL;
        doRp_AutoContacts( _gcell, rpM3, unusedContact, rpContact, DoTargetContact );
        _northEastContact = AutoContactVTee::create( _gcell, _net, viaLayer1 );
        AutoSegment::create( rpContact, _northEastContact, Flags::Vertical );
      }
    } else {
    // All RoutingPad are M1.
      Component* southWestRp = _routingPads[0];
      cdebug_log(145,0) << "| Initial S-W Global RP: " << southWestRp << endl;
      for ( size_t i=1 ; i<_routingPads.size() ; ++i ) {
        if (southWestRp->getBoundingBox().getHeight() >= 4*Session::getPitch(1)) break;
        if (_routingPads[i]->getBoundingBox().getHeight() > southWestRp->getBoundingBox().getHeight()) {
          cdebug_log(145,0) << "| Better RP: " << southWestRp << endl;
          southWestRp = _routingPads[i];
        }
      }

      if (west() and not south()) {
        _southWestContact = doRp_Access( _gcell, southWestRp, HAccess );
      } else if (not west() and south()) {
        AutoContact* rpContact = doRp_Access( _gcell, southWestRp, HAccess );
        _southWestContact = AutoContactTurn::create( _gcell, _net, viaLayer1 );
        AutoSegment::create( rpContact, _southWestContact, Flags::Horizontal );
      } else if (west() and south()) {
        AutoContact* rpContact = doRp_Access( _gcell, southWestRp, HAccess );
        _southWestContact = AutoContactHTee::create( _gcell, _net, viaLayer1 );
        AutoSegment::create( rpContact, _southWestContact, Flags::Horizontal );
      }

      Component* northEastRp = _routingPads[_routingPads.size()-1];
      cdebug_log(145,0) << "| Initial N-E Global RP: " << northEastRp << endl;

      if (_routingPads.size() > 1) {
        for ( size_t i=_routingPads.size()-1; i != 0 ; ) {
          i -= 1;
          if (northEastRp->getBoundingBox().getHeight() >= 4*Session::getPitch(1)) break;
          if (_routingPads[i]->getBoundingBox().getHeight() > northEastRp->getBoundingBox().getHeight()) {
            cdebug_log(145,0) << "| Better RP: " << northEastRp << endl;
            northEastRp = _routingPads[i];
          }
        } 
      }

      if (east() and not north()) {
        _northEastContact = doRp_Access( _gcell, northEastRp, HAccess );
      } else if (not east() and north()) {
        AutoContact* rpContact = doRp_Access( _gcell, northEastRp, HAccess );
        _northEastContact = AutoContactTurn::create( _gcell, _net, viaLayer1 );
        AutoSegment::create( rpContact, _northEastContact, Flags::Horizontal );
      } else if (east() and north()) {
        AutoContact* rpContact = doRp_Access( _gcell, northEastRp, HAccess );
        _northEastContact = AutoContactHTee::create( _gcell, _net, viaLayer1 );
        AutoSegment::create( rpContact, _northEastContact, Flags::Horizontal );
      }
    }

    cdebug_tabw(145,-1);
  }


  void  GCellTopology::_do_4G_1M2 ()
  {
    cdebug_log(145,1) << "_do_4G_1M2() [Managed Configuration]" << endl;

    Component* rpL2 = _routingPads[0];
    cdebug_log(145,0) << "rpL2 := " << rpL2 << endl;

    AutoContact* rpL2ContactSource = NULL;
    AutoContact* rpL2ContactTarget = NULL;

    doRp_AutoContacts( _gcell, rpL2, rpL2ContactSource, rpL2ContactTarget, DoSourceContact|DoTargetContact );

    const Layer* viaLayer2 = Session::getContactLayer(2);

    _southWestContact = AutoContactHTee::create( _gcell, _net, viaLayer2 );
    _northEastContact = AutoContactHTee::create( _gcell, _net, viaLayer2 );

    AutoSegment::create( _southWestContact, rpL2ContactSource, Flags::Horizontal );
    AutoSegment::create( rpL2ContactTarget, _northEastContact, Flags::Horizontal );

    cdebug_tabw(145,-1);
  }


  void  GCellTopology::_do_xG_xM2 ()
  {
    cdebug_log(145,1) << "_do_"
                      << (int)_connexity.fields.globals << "G_"
                      << (int)_connexity.fields.M2 << "M2() [Managed Configuration - x]" << endl;

    Component* biggestRp = _routingPads[0];
    for ( size_t i=1 ; i<_routingPads.size() ; ++i ) {
      doRp_StairCaseH( _gcell, _routingPads[i-1], _routingPads[i] );
      if (_routingPads[i]->getBoundingBox().getWidth() > biggestRp->getBoundingBox().getWidth())
        biggestRp = _routingPads[i];
    }

    const Layer* viaLayer1 = Session::getContactLayer(1);
    AutoContact* unusedContact = NULL;

    if (west() and not south()) {
      doRp_AutoContacts( _gcell, _routingPads[0], _southWestContact, unusedContact, DoSourceContact );
    } else if (not west() and south()) {
      _southWestContact = doRp_Access( _gcell, biggestRp, NoFlags );
    } else if (west() and south()) {
      AutoContact* rpContact = doRp_Access( _gcell, biggestRp, NoFlags );
      _southWestContact = AutoContactVTee::create( _gcell, _net, viaLayer1 );
      AutoSegment::create( rpContact, _southWestContact, Flags::Vertical );
    }

    if (east() and not north()) {
      doRp_AutoContacts( _gcell, _routingPads[_routingPads.size()-1], _northEastContact, unusedContact, DoSourceContact );
    } else if (not east() and north()) {
      _northEastContact = doRp_Access( _gcell, biggestRp, NoFlags );
    } else if (east() and north()) {
      AutoContact* rpContact = doRp_Access( _gcell, biggestRp, NoFlags );
      _northEastContact = AutoContactVTee::create( _gcell, _net, viaLayer1 );
      AutoSegment::create( rpContact, _northEastContact, Flags::Vertical );
    }

    cdebug_tabw(145,-1);
  }


  void  GCellTopology::_do_1G_1M3 ()
  {
    cdebug_log(145,1) << "_do_1G_1M3() [Optimised Configuration]" << endl;

    uint64_t flags = (east() or west()) ? HAccess : NoFlags;
    flags |= (north()) ? DoTargetContact : NoFlags;
    flags |= (south()) ? DoSourceContact : NoFlags;

    doRp_AutoContacts( _gcell
                     , _routingPads[0]
                     , _southWestContact
                     , _northEastContact
                     , flags
                     );
    if (not _southWestContact) _southWestContact = _northEastContact;
    if (not _northEastContact) _northEastContact = _southWestContact;

    cdebug_log(145,0) << "_southWest: " << _southWestContact << endl;
    cdebug_log(145,0) << "_northEast: " << _northEastContact << endl;

    const Layer* viaLayer1 = Session::getContactLayer(1);

    if (flags & HAccess) {
    // HARDCODED VALUE.
      if (_routingPads[0]->getBoundingBox().getHeight() < 3*Session::getPitch(1)) {
        AutoContact* subContact = AutoContactTurn::create( _gcell, _net, viaLayer1 );
        AutoSegment::create( _southWestContact, subContact, Flags::Vertical );

        _southWestContact = _northEastContact = subContact;
      }
    } else {
      if (_sourceContact) {
        if (_sourceContact->getX() != _southWestContact->getX()) {
          AutoContactTurn* turn1 = AutoContactTurn::create( _gcell, _net, viaLayer1 );
          AutoContactTurn* turn2 = AutoContactTurn::create( _gcell, _net, viaLayer1 );
          AutoSegment::create( _southWestContact, turn1, Flags::Vertical );
          AutoSegment::create( turn1            , turn2, Flags::Horizontal );
          _southWestContact = _northEastContact = turn2;
        }
      }
    }
    cdebug_tabw(145,-1);
  }


  void  GCellTopology::_do_xG_xM3 ()
  {
    cdebug_log(145,1) << "_do_xG_" << (int)_connexity.fields.M3
                      << "M3() [Managed Configuration]" << endl;
    cdebug_log(145,0) << "west:"  << west()  << endl;
    cdebug_log(145,0) << "east:"  << east()  << endl;
    cdebug_log(145,0) << "south:" << south() << endl;
    cdebug_log(145,0) << "north:" << north() << endl;

    sort( _routingPads.begin(), _routingPads.end(), SortRpByY(NoFlags) ); // increasing Y.
    for ( size_t i=1 ; i<_routingPads.size() ; i++ ) {
      doRp_StairCaseV( _gcell, _routingPads[i-1], _routingPads[i] );
    }

    const Layer* viaLayer1     = Session::getContactLayer(1);
    AutoContact* unusedContact = NULL;
    Component*   rp            = _routingPads[0];

    if (west() and not south()) {
      _southWestContact = doRp_Access( _gcell, rp, HAccess );
    } else if (not west() and south()) {
      doRp_AutoContacts( _gcell, rp, _southWestContact, unusedContact, DoSourceContact );
      if (_sourceContact) {
        if (_sourceContact->getX() != _southWestContact->getX()) {
          cdebug_log(149,0) << "Misaligned South: _source:" << DbU::getValueString(_sourceContact->getX())
                      << "_southWest:"                << DbU::getValueString(_southWestContact->getX()) << endl;

          AutoContactTurn* turn1 = AutoContactTurn::create( _gcell, _net, viaLayer1 );
          AutoContactTurn* turn2 = AutoContactTurn::create( _gcell, _net, viaLayer1 );
          AutoSegment::create( _southWestContact, turn1, Flags::Vertical );
          AutoSegment::create( turn1            , turn2, Flags::Horizontal );
          _southWestContact = turn2;
        }
      }
    } else if (west() and south()) {
      AutoContact* rpContact = NULL;
      doRp_AutoContacts( _gcell, rp, rpContact, unusedContact, DoSourceContact );
      _southWestContact = AutoContactVTee::create( _gcell, _net, viaLayer1 );
      AutoSegment::create( rpContact, _southWestContact, Flags::Vertical );
    }

    rp = _routingPads[_routingPads.size()-1];
    if (east() and not north()) {
      _northEastContact = doRp_Access( _gcell, rp, HAccess );
    } else if (not east() and north()) {
      doRp_AutoContacts( _gcell, rp, unusedContact, _northEastContact, DoTargetContact );
      if (_sourceContact) {
        if (_sourceContact->getX() != _northEastContact->getX()) {
          cdebug_log(149,0) << "Misaligned North: _source:" << DbU::getValueString(_sourceContact->getX())
                      << "_southWest:"                << DbU::getValueString(_northEastContact->getX()) << endl;

          AutoContactTurn* turn1 = AutoContactTurn::create( _gcell, _net, viaLayer1 );
          AutoContactTurn* turn2 = AutoContactTurn::create( _gcell, _net, viaLayer1 );
          AutoSegment::create( _northEastContact, turn1, Flags::Vertical   );
          AutoSegment::create( turn1            , turn2, Flags::Horizontal );
          _northEastContact = turn2;
        }
      }
    } else if (east() and north()) {
      AutoContact* rpContact = NULL;
      doRp_AutoContacts( _gcell, rp, unusedContact, rpContact, DoTargetContact );
      _northEastContact = AutoContactVTee::create( _gcell, _net, viaLayer1 );
      AutoSegment::create( rpContact, _northEastContact, Flags::Vertical );
    }

    cdebug_tabw(145,-1);
  }


  void  singleGCell ( AnabaticEngine* anbt, Net* net )
  {
    cdebug_log(145,1) << "singleGCell() " << net << endl;

    vector<Component*>  rpM1s;
    Component*          rpM2 = NULL;

    forEach ( RoutingPad*, irp, net->getRoutingPads() ) {
      if (Session::getRoutingGauge()->getLayerDepth(irp->getLayer()) == 1)
        rpM2 = *irp;
      else
        rpM1s.push_back( *irp );
    }

    if ((rpM1s.size() < 2) and not rpM2) {
      cerr << Error( "For %s, less than two Plugs/Pins (%d)."
                   , getString(net).c_str()
                   , rpM1s.size() ) << endl;
      cdebug_tabw(145,-1);
      return;
    }

    if (rpM1s.empty()) {
      cerr << Error( "No METAL1 in Single GCell for Net \"%s\".", getString(net->getName()).c_str() ) << endl;
      cdebug_tabw(145,-1);
      return;
    }

    sort( rpM1s.begin(), rpM1s.end(), SortRpByX(NoFlags) ); // increasing X.

    GCell* gcell1 = anbt->getGCellUnder( (*rpM1s.begin ())->getCenter() );
    GCell* gcell2 = anbt->getGCellUnder( (*rpM1s.rbegin())->getCenter() );

    if (not gcell1) {
      cerr << Error( "No GCell under %s.", getString(rpM1s[0]).c_str() ) << endl;
      cdebug_tabw(145,-1);
      return;
    }
    if (gcell1 != gcell2) {
      cerr << Error( "Not under a single GCell %s.", getString(rpM1s[0]).c_str() ) << endl;
      cdebug_tabw(145,-1);
      return;
    }

    cdebug_log(145,0) << "singleGCell " << gcell1 << endl;

    AutoContact* turn   = NULL;
    AutoContact* source = NULL;
    AutoContact* target = NULL;

    for ( size_t irp=1 ; irp<rpM1s.size() ; ++irp ) {
      GCellTopology::doRp_AutoContacts( gcell1, rpM1s[irp-1], source, turn, DoSourceContact );
      GCellTopology::doRp_AutoContacts( gcell1, rpM1s[irp  ], target, turn, DoSourceContact );
      AutoSegment::create( source, target, Flags::Horizontal );
    }

    if (rpM2) {
      GCellTopology::doRp_AutoContacts( gcell1, rpM1s[0], source, turn, DoSourceContact );
      GCellTopology::doRp_AutoContacts( gcell1, rpM2    , target, turn, DoSourceContact );
      turn = AutoContactTurn::create( gcell1, rpM2->getNet(), Session::getContactLayer(1) );
      AutoSegment::create( source, turn  , Flags::Horizontal );
      AutoSegment::create( turn  , target, Flags::Vertical );
    }

    cdebug_tabw(145,-1);
  }


  bool isVertical ( RoutingPad* rp )
  {
    return (  (rp->getSourcePosition().getX() == rp->getTargetPosition().getX()) 
           && (rp->getSourcePosition().getY() != rp->getTargetPosition().getY()) 
           );
  }


  bool isHorizontal ( RoutingPad* rp )
  {
    return (  (rp->getSourcePosition().getY() == rp->getTargetPosition().getY()) 
           && (rp->getSourcePosition().getX() != rp->getTargetPosition().getX()) 
           );
  }


  RoutingPad* returnSW ( GCell* gcell, RoutingPad* rp1, RoutingPad* rp2 )
  {
    DbU::Unit c1SW = (rp1->getSourcePosition().getX() - gcell->getXMin()) + (rp1->getSourcePosition().getY() - gcell->getYMin());
    DbU::Unit c2SW = (rp2->getSourcePosition().getX() - gcell->getXMin()) + (rp2->getSourcePosition().getY() - gcell->getYMin());

    if ( c1SW < c2SW ){
      return rp1;
    } else {
      return rp2;
    }
  }


  RoutingPad* returnNE ( GCell* gcell, RoutingPad* rp1, RoutingPad* rp2 )
  {
    DbU::Unit c1NE = (gcell->getXMax() - rp1->getTargetPosition().getX()) + (gcell->getYMax() - rp1->getTargetPosition().getY());
    DbU::Unit c2NE = (gcell->getXMax() - rp2->getTargetPosition().getX()) + (gcell->getYMax() - rp2->getTargetPosition().getY());

    if ( c1NE < c2NE ){
      return rp1;
    } else {
      return rp2;
    }
  }
  

  AutoContact* GCellTopology::_doDevice ()
  {
    cdebug_log(145,1) << "void  GCellTopology::_doDevice ()" << _gcell << endl;

    for ( size_t i=0; i<_routingPads.size() ; i++ ) {
      if (   (_routingPads[i]->getSourcePosition().getX() == _routingPads[i]->getTargetPosition().getX())
         and (_routingPads[i]->getSourcePosition().getY() == _routingPads[i]->getTargetPosition().getY()) ) {
        throw Error( "GCellTopology::_doDevice() Some RoutingPads are not set to a component.\n"
                     "        On: %s."
                   , getString(_gcell).c_str() );
      }
    }
    cdebug_log(145,0) << "FromHook: " << _fromHook << endl;
    cdebug_log(145,0) << "North   : " << north()   << endl;
    cdebug_log(145,0) << "East    : " << east()    << endl;
    cdebug_log(145,0) << "South   : " << south()   << endl;
    cdebug_log(145,0) << "West    : " << west()    << endl;
    cdebug_log(145,0) << "_routingPads.size(): " << _routingPads.size() << endl;

    RoutingPad*  rpNE = NULL;
    RoutingPad*  rpSW = NULL;
    _southWestContact = NULL;

    if (_routingPads.size() > 1) {
      cdebug_log(145,0) << "Case _routingPads.size() > 1 "<< endl;
      for ( RoutingPad* rp : _routingPads )
        cdebug_log(145,0) << rp << endl;
      
    // #1: Find RoutingPads to use for AutoContacts NE+SW
      rpNE = _routingPads[0];
      rpSW = _routingPads[0];

      for ( size_t i=1 ; i<_routingPads.size() ; i++ ) {
        rpNE = returnNE( _gcell, rpNE, _routingPads[i] );
        rpSW = returnSW( _gcell, rpSW, _routingPads[i] );
      }
      
      cdebug_log(145,0) << "rpNE: " << rpNE << endl;
      cdebug_log(145,0) << "rpSW: " << rpSW << endl;
    } else if (_routingPads.size() == 0) {
      cdebug_log(145,0) << "Case _routingPads.size() = 0 "<< endl;
      throw Error( "GCellTopology::_doDevice() No RoutingPads found.\n"
                   "        On: %s."
                 , getString(_gcell).c_str() );
    } else {
      cdebug_log(145,0) << "Case _routingPads.size() = 1 "<< endl;
      rpNE = rpSW = _routingPads[0];
    }
    cdebug_log(145,0) << "rp NE: " << rpNE << endl;
    cdebug_log(145,0) << "rp SW: " << rpSW << endl;

    if (rpNE and rpSW) {
      if (east()) {
        cdebug_log(145,0) << "East"  << endl;
        AutoContact* ac = doRp_AccessAnalog( _gcell, rpNE, NoFlags );
        push( east(), ac, SouthWest );
      } 
      if (west()) {
        cdebug_log(145,0) << "West"  << endl;
        AutoContact* ac = doRp_AccessAnalog( _gcell, rpSW, NoFlags );
        push( west(), ac, SouthWest );
      }
      if (south()) {
        cdebug_log(145,0) << "South"  << endl;
        AutoContact* ac = doRp_AccessAnalog( _gcell, rpSW, NoFlags );
        push( south(), ac, SouthWest );
      }
      if (north()){
        cdebug_log(145,0) << "North"  << endl;
        AutoContact* ac = doRp_AccessAnalog( _gcell, rpNE, NoFlags );
        push( north(), ac, SouthWest );
      }
    }
    cdebug_log(145,0) << "doDevice done" << endl;
    cdebug_tabw(145,-1);

    return _southWestContact;
  }


  AutoContact*  GCellTopology::_doHChannel ()
  {
    cdebug_log(145,1) << "void  GCellTopology::_doHChannel() " << _gcell << endl;
    
    vector<Hook*>  hooks;
    Hook*          firsthhook = NULL;
    Hook*          lasthhook  = NULL;
    AutoContact*   targetContact = NULL;
    
  // Save segments only
    cdebug_log(145,0) << "fromHook: "  << _fromHook << endl;
    for ( Hook* toHook : _fromHook->getHooks() ) {
      cdebug_log(145,0) << "toHook: "  << toHook << endl;

      Segment* s = dynamic_cast<Segment*>( toHook->getComponent() );
      if (s) {
        Horizontal* h = dynamic_cast<Horizontal*>(s);
        if (h) {
          if      (h->getSource()->getX() <= _gcell->getXMin()) firsthhook = toHook;
          else if (h->getTarget()->getX() >= _gcell->getXMax()) lasthhook  = toHook;
          else    cdebug_log(145,0) << "Error(AutoContact*  GCellTopology::_doHChannel ( ForkStack& forks )): This case should not happen " << endl;
        } else hooks.push_back(toHook);
      }
    }

    cdebug_log(145,0) << "Sorted hooks:" << endl;
    sort( hooks.begin(), hooks.end(), SortHookByX(NoFlags) );
    
    if (firsthhook) hooks.insert   (hooks.begin(), firsthhook);
    if (lasthhook ) hooks.push_back(lasthhook                );

    for (vector<Hook*>::iterator it = hooks.begin(); it != hooks.end(); it++){
      cdebug_log(145,0) << "toHook: "  << (*it) << endl;
    }
    size_t i = 0;

  // More than 2 AutoContacts to create
    cdebug_log(145,0) << "Number of hooks: " <<  hooks.size() << endl;
    if ( hooks.size() > 2 ){
      cdebug_log(145,0) << "Number of hooks > 2 : " <<  hooks.size() << endl;
      vector<AutoContact*> autoContacts;
      bool                 firstH      = false; 

      for (vector<Hook*>::iterator it = hooks.begin(); it != hooks.end(); it++){
      
        Horizontal* h = dynamic_cast<Horizontal*>((*it)->getComponent());
        Vertical*   v = dynamic_cast<Vertical*>  ((*it)->getComponent());
        AutoContact* ac = NULL;
        cdebug_log(145,0) << "index: " << i << endl;
        cdebug_log(145,0) << "h    : " << h << endl;
        cdebug_log(145,0) << "v    : " << v << endl;

        if (i == 0){
          if        (h){
            cdebug_log(145,0) << "case i=0, h" <<  endl;
            ac = AutoContactHTee::create( _gcell, _net, Session::getContactLayer(2) );
            ac->setX(_gcell->getXMin());
            ac->setY(_gcell->getYMin() + _gcell->getHeight()/2);
            firstH = true;
          } else if (v){
            cdebug_log(145,0) << "case i=0, v" <<  endl;
            ac = AutoContactTurn::create( _gcell, _net, Session::getContactLayer(1) );
            ac->setX(v->getX());
            ac->setY(_gcell->getYMin() + _gcell->getHeight()/2);
          }
        } else if (i == hooks.size()-1){
          if        (h){
            cdebug_log(145,0) << "case i=last, h" <<  endl;
            ac = autoContacts[i-1];
          } else if (v){
            cdebug_log(145,0) << "case i=last, v" <<  endl;
            ac = AutoContactTurn::create( _gcell, _net, Session::getContactLayer(1) );
            ac->setX(v->getX());
            ac->setY(_gcell->getYMin() + _gcell->getHeight()/2);
          }
        } else {
          if ((i == 1)&&(firstH)){
            cdebug_log(145,0) << "case i=1, first was H" <<  endl;
            ac = autoContacts[i-1];
            if (v) ac->setX(v->getX());
            else {
              cerr << Warning( "Something is wrong with the number of globals in this HChannel."
                             , getString(_gcell).c_str() )
                   << endl;
            }
          } else {
            cdebug_log(145,0) << "case i=1, first was V" <<  endl;
            ac = AutoContactHTee::create( _gcell, _net, Session::getContactLayer(2) );
            ac->setX(v->getX());
            ac->setY(_gcell->getYMin() + _gcell->getHeight()/2);
          }
        } 
        cdebug_log(145,0) << "[Create AutoContact]: " << ac << endl;
        autoContacts.push_back(ac);
      
        cdebug_log(145,0) << "FromHook: " << _fromHook << endl;
        if (_fromHook->getComponent() == (*it)->getComponent()){
          cdebug_log(145,0) << "Found from:" << (*it)->getComponent() << endl;
          targetContact = ac;
        } else {
          push( (*it), ac );
        }
        i++;
      }   
      cdebug_log(145,0) << "Chain contacts: "  << endl;
      for (size_t j=1; j < autoContacts.size(); j++){
        if (autoContacts[j-1] != autoContacts[j]){
          AutoSegment* segment = AutoSegment::create( autoContacts[j-1] , autoContacts[j], Flags::Horizontal );
          cdebug_log(145,0) << "[Create global segment (2)]: " << segment << endl;
        }
      }
    // There are only 2 AutoContacts to create
    } else if (hooks.size() == 2){
      cdebug_log(145,0) << "Number of hooks == 2 : " <<  hooks.size() << endl;
      Horizontal*  h0  = dynamic_cast<Horizontal*>(hooks[0]->getComponent());
      Vertical*    v0  = dynamic_cast<Vertical*>  (hooks[0]->getComponent());
      Horizontal*  h1  = dynamic_cast<Horizontal*>(hooks[1]->getComponent());
      Vertical*    v1  = dynamic_cast<Vertical*>  (hooks[1]->getComponent());
      AutoContact* source = NULL;
      AutoContact* target = NULL;
      cdebug_log(145,0) << "h0: " << h0 << endl;
      cdebug_log(145,0) << "v0: " << v0 << endl;
      cdebug_log(145,0) << "h1: " << h1 << endl;
      cdebug_log(145,0) << "v1: " << v1 << endl;
        
      if ((v0 != NULL) && (v1 != NULL)){
        cdebug_log(145,0) << "case 2V" << endl;
        source = AutoContactTurn::create( _gcell, _net, Session::getContactLayer(1) );
        source->setX(v0->getX());
        source->setY(_gcell->getYMin() + _gcell->getHeight()/2);
        target = AutoContactTurn::create( _gcell, _net, Session::getContactLayer(1) );
        target->setX(v1->getX());
        target->setY(_gcell->getYMin() + _gcell->getHeight()/2);

        cdebug_log(145,0) << "[Create AutoContact Source]: " << source << endl;
        cdebug_log(145,0) << "[Create AutoContact Target]: " << target << endl;
        AutoSegment* globalSegment = AutoSegment::create( source, target, Flags::Horizontal );
        cdebug_log(145,0) << "[Create global segment (3)]: " << globalSegment << endl;
        
        if        (_fromHook->getComponent() == hooks[0]->getComponent()){
          cdebug_log(145,0) << "Found from:" << hooks[0]->getComponent() << endl;
          targetContact = source;
          push( hooks[1], target );
          
        } else if (_fromHook->getComponent() == hooks[1]->getComponent()){
          cdebug_log(145,0) << "Found from:" << hooks[1]->getComponent() << endl;
          targetContact = target;
          push( hooks[0], source );
        }

      } else if (((h0 != NULL) && (v1 != NULL)) || ((v0 != NULL) && (h1 != NULL))){
        cdebug_log(145,0) << "case 1V and 1H" << endl;
        source = AutoContactTurn::create( _gcell, _net, Session::getContactLayer(1) );
        cdebug_log(145,0) << "[Create AutoContact]: " << source << endl;
        if (h0 && v1){
          source->setX(v1->getX());
        } else {
          source->setX(v0->getX());
        }
        source->setY(_gcell->getYMin() + _gcell->getHeight()/2);
        targetContact = source;

        if        (_fromHook->getComponent() == hooks[0]->getComponent()){
          cdebug_log(145,0) << "Found from:" << hooks[0]->getComponent() << endl;
          push( hooks[1], source );
          
        } else if (_fromHook->getComponent() == hooks[1]->getComponent()){
          cdebug_log(145,0) << "Found from:" << hooks[1]->getComponent() << endl;
          push( hooks[0], source );
        }
      } else if ((h0 != NULL) && (h1 != NULL)){
        cdebug_log(145,0) << "case 2H" << endl;
        source = AutoContactTurn::create( _gcell, _net, Session::getContactLayer(1) );
        source->setX(_gcell->getXMin() + _gcell->getWidth()/2);
        source->setY(_gcell->getYMin() + _gcell->getHeight()/2);
        target = AutoContactTurn::create( _gcell, _net, Session::getContactLayer(1) );
        target->setX(_gcell->getXMin() + _gcell->getWidth()/2);
        target->setY(_gcell->getYMin() + _gcell->getHeight()/2);

        cdebug_log(145,0) << "[Create AutoContact Source]: " << source << endl;
        cdebug_log(145,0) << "[Create AutoContact Target]: " << target << endl;

        AutoSegment* globalSegment = AutoSegment::create( source, target, Flags::Vertical );
        cdebug_log(145,0) << "[Create global segment (4)]: " << globalSegment << endl;

        if        (_fromHook->getComponent() == hooks[0]->getComponent()){
          cdebug_log(145,0) << "Found from:" << hooks[0]->getComponent() << endl;
          targetContact = source;
          push( hooks[1], target );
          
        } else if (_fromHook->getComponent() == hooks[1]->getComponent()){
          cdebug_log(145,0) << "Found from:" << hooks[1]->getComponent() << endl;
          targetContact = target;
          push( hooks[0], source );
        }
      }
    }
    cdebug_log(145,0) << "doHChannel done" << endl;
    cdebug_tabw(145,-1);

    return targetContact;
  }


  AutoContact*  GCellTopology::_doVChannel ()
  {
    cdebug_log(145,1) << "void  GCellTopology::_doVChannel ()" << _gcell << endl;
    
  
    vector<Hook*>  hooks;
    Hook*          firstvhook = NULL;
    Hook*          lastvhook  = NULL;
    AutoContact*   targetContact   = NULL;
    
  // Save segments only
    cdebug_log(145,0) << "fromHook: "  << _fromHook << endl;

    
    for ( Hook* toHook : _fromHook->getHooks() ) {
      cdebug_log(145,0) << "toHook: "  << toHook << endl;

      Segment* s = dynamic_cast<Segment*>( toHook->getComponent() );
      if (s) {
        Vertical* v = dynamic_cast<Vertical*>(s);
        if (v) {
          if      (v->getSource()->getY() <= _gcell->getYMin()) firstvhook = toHook;
          else if (v->getTarget()->getY() >= _gcell->getYMax()) lastvhook  = toHook;
          else    cdebug_log(145,0) << "Error(AutoContact*  GCellTopology::_doVChannel()): This case should not happen " << endl;
        } else hooks.push_back(toHook);
      }
    }
    
    cdebug_log(145,0) << "Sorted hooks:" << endl;
    sort( hooks.begin(), hooks.end(), SortHookByY(NoFlags) );

    if (firstvhook) hooks.insert   (hooks.begin(), firstvhook);
    if (lastvhook ) hooks.push_back(lastvhook                );

    for (vector<Hook*>::iterator it = hooks.begin(); it != hooks.end(); it++){
      cdebug_log(145,0) << "toHook: "  << (*it) << endl;
    }
    size_t i = 0;

  // More than 2 AutoContacts to create
    cdebug_log(145,0) << "Number of hooks: " <<  hooks.size() << endl;
    if ( hooks.size() > 2 ){
      cdebug_log(145,0) << "Number of hooks > 2 : " <<  hooks.size() << endl;
      vector<AutoContact*> autoContacts;
      bool                 firstV      = false; 

      for (vector<Hook*>::iterator it = hooks.begin(); it != hooks.end(); it++){
      
        Horizontal* h = dynamic_cast<Horizontal*>((*it)->getComponent());
        Vertical*   v = dynamic_cast<Vertical*>  ((*it)->getComponent());
        AutoContact* ac = NULL;
        cdebug_log(145,0) << "index: " << i << endl;
        cdebug_log(145,0) << "h    : " << h << endl;
        cdebug_log(145,0) << "v    : " << v << endl;

        if (i == 0){
          if        (v){
            cdebug_log(145,0) << "case i=0, v"<< endl;
            ac = AutoContactVTee::create( _gcell, _net, Session::getContactLayer(1) );
            ac->setX(_gcell->getXMin() + _gcell->getWidth()/2);
            ac->setY(_gcell->getYMin());
            firstV = true;
          } else if (h){
            cdebug_log(145,0) << "case i=0, h"<< endl;
            ac = AutoContactTurn::create( _gcell, _net, Session::getContactLayer(1) );
            ac->setX(_gcell->getXMin() + _gcell->getWidth()/2);
            ac->setY(h->getY());
          }
        } else if (i == hooks.size()-1){
          if        (v){
            cdebug_log(145,0) << "case i=last, v"<< endl;
            ac = autoContacts[i-1];
          } else if (h){
            cdebug_log(145,0) << "case i=last, h"<< endl;
            ac = AutoContactTurn::create( _gcell, _net, Session::getContactLayer(1) );
            ac->setX(_gcell->getXMin() + _gcell->getWidth()/2);
            ac->setY(h->getY());
          }
        } else {
          if ((i == 1)&&(firstV)){
            cdebug_log(145,0) << "case i=X, first was V"<< endl;
            ac = autoContacts[i-1];
            if (h) ac->setY(h->getY());
            else {
              cerr << Warning( "Something is wrong with the number of globals in this VChannel."
                             , getString(_gcell).c_str() )
                   << endl;
            }
          } else {
            cdebug_log(145,0) << "case i=X, first was H"<< endl;
            ac = AutoContactVTee::create( _gcell, _net, Session::getContactLayer(1) );
            ac->setX(_gcell->getXMin() + _gcell->getWidth()/2);
            ac->setY(h->getY());
          }
        } 
        cdebug_log(145,0) << "[Create AutoContact]: " << ac << endl;
        autoContacts.push_back(ac);
      
        cdebug_log(145,0) << "FromHook: " << _fromHook << endl << endl;
        if (_fromHook->getComponent() == (*it)->getComponent()){
          cdebug_log(145,0) << "Found from:" << (*it)->getComponent() << endl;
          targetContact = ac;
        } else {
          push( (*it), ac );
        }
        i++;
      }   
      cdebug_log(145,0) << "Chain contacts: "  << endl;
      for (size_t j=1; j < autoContacts.size(); j++){
        if (autoContacts[j-1] != autoContacts[j]){
          AutoSegment* segment = AutoSegment::create( autoContacts[j-1] , autoContacts[j], Flags::Vertical );
          if (not segment->isGlobal()) segment->setFlags( AutoSegment::SegLongLocal );
          cdebug_log(145,0) << "[Create global segment (5)]: " << segment << endl;
        }
      }
    // There are only 2 AutoContacts to create
    } else if (hooks.size() == 2){
      cdebug_log(145,0) << "Number of hooks == 2 : " <<  hooks.size() << endl;
      Horizontal*  h0  = dynamic_cast<Horizontal*>(hooks[0]->getComponent());
      Vertical*    v0  = dynamic_cast<Vertical*>  (hooks[0]->getComponent());
      Horizontal*  h1  = dynamic_cast<Horizontal*>(hooks[1]->getComponent());
      Vertical*    v1  = dynamic_cast<Vertical*>  (hooks[1]->getComponent());
      AutoContact* source = NULL;
      AutoContact* target = NULL;
      cdebug_log(145,0) << "h0: " << h0 << endl;
      cdebug_log(145,0) << "v0: " << v0 << endl;
      cdebug_log(145,0) << "h1: " << h1 << endl;
      cdebug_log(145,0) << "v1: " << v1 << endl;
          
      if ((h0 != NULL) && (h1 != NULL)){
        cdebug_log(145,0) << "case 2H" << endl;
        source = AutoContactTurn::create( _gcell, _net, Session::getContactLayer(1) );
        source->setX(_gcell->getXMin() + _gcell->getWidth()/2);
        source->setY(h0->getY());

        target = AutoContactTurn::create( _gcell, _net, Session::getContactLayer(1) );
        target->setX(_gcell->getXMin() + _gcell->getWidth()/2);
        target->setY(h1->getY());

        cdebug_log(145,0) << "[Create AutoContact Source]: " << source << endl;
        cdebug_log(145,0) << "[Create AutoContact Target]: " << target << endl;

        AutoSegment* globalSegment = AutoSegment::create( source, target, Flags::Vertical );
        if (not globalSegment->isGlobal()) globalSegment->setFlags( AutoSegment::SegLongLocal );
        cdebug_log(145,0) << "[Create global segment (6)]: " << globalSegment << endl;

        if        (_fromHook->getComponent() == hooks[0]->getComponent()){
          cdebug_log(145,0) << "Found from:" << hooks[0]->getComponent() << endl;
          targetContact = source;
          push( hooks[1], target );
          
        } else if (_fromHook->getComponent() == hooks[1]->getComponent()){
          cdebug_log(145,0) << "Found from:" << hooks[1]->getComponent() << endl;
          targetContact = target;
          push( hooks[0], source );
        }

      } else if (((v0 != NULL) && (h1 != NULL)) || ((h0 != NULL) && (v1 != NULL))){
        cdebug_log(145,0) << "case 1V and 1H" << endl;
        source = AutoContactTurn::create( _gcell, _net, Session::getContactLayer(1) );
        if (v0 && h1){
          source->setY(h1->getY());
        } else {
          source->setY(h0->getY());
        }
        source->setX(_gcell->getXMin() + _gcell->getWidth()/2);
        targetContact = source;

        cdebug_log(145,0) << "[Create AutoContact]: " << source << endl;

        if        (_fromHook->getComponent() == hooks[0]->getComponent()){
          cdebug_log(145,0) << "Found from:" << hooks[0]->getComponent() << endl;
          push( hooks[1], source );
          
        } else if (_fromHook->getComponent() == hooks[1]->getComponent()){
          cdebug_log(145,0) << "Found from:" << hooks[1]->getComponent() << endl;
          push( hooks[0], source );
        }
      } else if ((v0 != NULL) && (v1 != NULL)){
        cdebug_log(145,0) << "case 2V" << endl;
        source = AutoContactTurn::create( _gcell, _net, Session::getContactLayer(1) );
        source->setX(_gcell->getXMin() + _gcell->getWidth()/2);
        source->setY(_gcell->getYMin() + _gcell->getHeight()/2);
        target = AutoContactTurn::create( _gcell, _net, Session::getContactLayer(1) );
        target->setX(_gcell->getXMin() + _gcell->getWidth()/2);
        target->setY(_gcell->getYMin() + _gcell->getHeight()/2);

        cdebug_log(145,0) << "[Create AutoContact Source]: " << source << endl;
        cdebug_log(145,0) << "[Create AutoContact Target]: " << target << endl;

        AutoSegment* globalSegment = AutoSegment::create( source, target, Flags::Horizontal );
        if (not globalSegment->isGlobal()) globalSegment->setFlags( AutoSegment::SegLongLocal );
        cdebug_log(145,0) << "[Create global segment (7)]: " << globalSegment << endl;

        if        (_fromHook->getComponent() == hooks[0]->getComponent()){
          cdebug_log(145,0) << "Found from:" << hooks[0]->getComponent() << endl;
          targetContact = source;
          push( hooks[1], target );
          
        } else if (_fromHook->getComponent() == hooks[1]->getComponent()){
          cdebug_log(145,0) << "Found from:" << hooks[1]->getComponent() << endl;
          targetContact = target;
          push( hooks[0], source );
        }
      }
    }
    cdebug_log(145,0) << "doVChannel done" << endl;
    cdebug_tabw(145,-1);

    return targetContact;
  }

  
  AutoContact* GCellTopology::_doHRail ()
  {
    cdebug_log(145,1) << "void  GCellTopology::_doHRail ()" << _gcell << endl;
    cdebug_log(145,0) << "FromHook: " << _fromHook << endl;
    cdebug_log(145,0) << "North   : " << north()   << endl;
    cdebug_log(145,0) << "East    : " << east()    << endl;
    cdebug_log(145,0) << "South   : " << south()   << endl;
    cdebug_log(145,0) << "West    : " << west()    << endl;
    cdebug_log(145,0) << "_routingPads.size(): " << _routingPads.size() << endl;

    RoutingPad*  rpNE = NULL;
    RoutingPad*  rpSW = NULL;
    _southWestContact = NULL;
    if (_routingPads.size() == 1){
      rpNE = rpSW = _routingPads[0];
    } else {
      cdebug_log(145,0) << "Case _routingPads.size() != 1 "<< endl;
      throw Error( "GCellTopology::_doHRail() Unexpected case.\n"
                   "        On: %s."
                 , getString(_gcell).c_str() );
    }
    cdebug_log(145,0) << "rp: " << _routingPads[0] << endl;
    AutoContact* ac = doRp_AccessAnalog( _gcell, rpNE, NoFlags );
    if (east()) {
      cdebug_log(145,0) << "East"  << endl;
      push( east(), ac, SouthWest );
    } 
    if (west()) {
      cdebug_log(145,0) << "West"  << endl;
      push( west(), ac, SouthWest );
    }
    if (south()) {
      cdebug_log(145,0) << "South"  << endl;
      push( south(), ac, SouthWest );
    }
    if (north()){
      cdebug_log(145,0) << "North"  << endl;
      push( north(), ac, SouthWest );
    }
    
    cdebug_log(145,0) << "doHRail done" << endl;
    cdebug_tabw(145,-1);
    
    return _southWestContact;
  }

  
  AutoContact* GCellTopology::_doVRail ()
  {
    cdebug_log(145,1) << "void  GCellTopology::_doVRail ()" << _gcell << endl;
    cdebug_log(145,0) << "FromHook: " << _fromHook << endl;
    cdebug_log(145,0) << "North   : " << north()   << endl;
    cdebug_log(145,0) << "East    : " << east()    << endl;
    cdebug_log(145,0) << "South   : " << south()   << endl;
    cdebug_log(145,0) << "West    : " << west()    << endl;
    cdebug_log(145,0) << "_routingPads.size(): " << _routingPads.size() << endl;

    RoutingPad*  rpNE = NULL;
    RoutingPad*  rpSW = NULL;
    _southWestContact = NULL;
    if (_routingPads.size() == 1){
      rpNE = rpSW = _routingPads[0];
    } else {
      cdebug_log(145,0) << "Case _routingPads.size() != 1 "<< endl;
      throw Error( "GCellTopology::_doVRail() Unexpected case.\n"
                   "        On: %s."
                 , getString(_gcell).c_str() );
    }
    cdebug_log(145,0) << "rp: " << _routingPads[0] << endl;
    AutoContact* ac = doRp_AccessAnalog( _gcell, rpNE, NoFlags );
    if (east()) {
      cdebug_log(145,0) << "East"  << endl;
      push( east(), ac, SouthWest );
    } 
    if (west()) {
      cdebug_log(145,0) << "West"  << endl;
      push( west(), ac, SouthWest );
    }
    if (south()) {
      cdebug_log(145,0) << "South"  << endl;
      push( south(), ac, SouthWest );
    }
    if (north()){
      cdebug_log(145,0) << "North"  << endl;
      push( north(), ac, SouthWest );
    }

    cdebug_log(145,0) << "doVRail done" << endl;
    cdebug_tabw(145,-1);
    
    return _southWestContact;
  }


  unsigned int GCellTopology::getNumberGlobals ()
  {
    unsigned int i = 0;
    if (north()) i++;
    if (south()) i++;
    if (east() ) i++;
    if (west() ) i++;
    return i;
  }


  unsigned int GCellTopology::getDeviceNeighbourBound()
  {
    unsigned int bound = 0;
    if (north()){
      if (_gcell->getNorth()->isDevice()) bound = NorthBound;
    } else if (south()){
      if (_gcell->getSouth()->isDevice()) bound = SouthBound;
    } else if (east()){
      if (_gcell->getEast()->isDevice() ) bound = EastBound;
    } else if (west()){
      if (_gcell->getWest()->isDevice() ) bound = WestBound;
    }
    return bound;
  }


  AutoContact*  GCellTopology::_doStrut ()
  {
    cdebug_log(145,1) << "void  GCellTopology::_doStrut ()" << _gcell << endl;

    AutoContact* targetContact   = NULL; // Contact for fromHook segment
    cdebug_log(145,0) << "FromHook: " << _fromHook  << endl;
    cdebug_log(145,0) << "North   : " << north()     << endl;
    cdebug_log(145,0) << "East    : " << east()      << endl;
    cdebug_log(145,0) << "South   : " << south()     << endl;
    cdebug_log(145,0) << "West    : " << west()      << endl;

  // Determine NE and SW contacts
    if ( getNumberGlobals() == 2 ){
      cdebug_log(145,0) << "Case 2 globals: " <<  getNumberGlobals() << endl;

      AutoContact* source = AutoContactTurn::create( _gcell, _net, Session::getContactLayer(1) );
      targetContact = source;
      if (  ((north() != NULL) && (west() != NULL)) 
         || ((north() != NULL) && (east() != NULL))
         || ((south() != NULL) && (west() != NULL))
         || ((south() != NULL) && (east() != NULL))
         ){
        if        ((north() != NULL) && (west() != NULL)) {
          cdebug_log(145,0) << "North: " << north() << endl;
          cdebug_log(145,0) << "West : " << west() << endl;
          source->setX(north()->getComponent()->getX());
          source->setY(west()->getComponent ()->getY());
        } else if ((north() != NULL) && (east() != NULL)) {
          cdebug_log(145,0) << "North: " << north() << endl;
          cdebug_log(145,0) << "East : " << east() << endl;
          source->setX(north()->getComponent()->getX());
          source->setY(east()->getComponent ()->getY());
        } else if ((south() != NULL) && (west() != NULL)) {
          cdebug_log(145,0) << "South: " << south() << endl;
          cdebug_log(145,0) << "West : " << west() << endl;
          source->setX(south()->getComponent()->getX());
          source->setY(west()->getComponent ()->getY());
        } else if ((south() != NULL) && (east() != NULL)) {
          cdebug_log(145,0) << "South: " << south() << endl;
          cdebug_log(145,0) << "East: "  << east() << endl;
          source->setX(south()->getComponent()->getX());
          source->setY(east()->getComponent ()->getY());
        }

        cdebug_log(145,0) << "[Create AutoContact]: " << source << endl;
        push( east() , source );
        push( west() , source );
        push( north(), source );
        push( south(), source );

      } else if ((north() != NULL) && (south() != NULL)) {
        cdebug_log(145,0) << "Case NS" <<  endl;
        AutoContact* target = AutoContactTurn::create( _gcell, _net, Session::getContactLayer(1) );
        source->setX(north()->getComponent()->getX());
        source->setY(_gcell->getYMin() + _gcell->getHeight()/2);
        target->setX(south()->getComponent()->getX());
        target->setY(_gcell->getYMin() + _gcell->getHeight()/2);

        if (north()->getComponent()->getX() > south()->getComponent()->getX()) {
          swap( source, target );
          if ( north() and (_fromHook != north()) ){
            push( north(), target );
          } else if ( south() and (_fromHook != south()) ){
            push( south(), source );
            targetContact = target;
          } else {
            cerr << Warning( "Something is wrong with the globals and the fromHook in this Strut 1."
                           , getString(_gcell).c_str() )
                 << endl;
          }
        } else {
          if ( north() and (_fromHook != north()) ){ 
            push( north(), source );
            targetContact = target;
          } else if ( south() and (_fromHook != south()) ){
            push( south(), target );
          } else {
            cerr << Warning( "Something is wrong with the globals and the fromHook in this Strut 2."
                           , getString(_gcell).c_str() )
                 << endl;
          }
        }
        cdebug_log(145,0) << "[Create AutoContact]: " << source << endl;
        cdebug_log(145,0) << "[Create AutoContact]: " << target << endl;

        AutoSegment* globalSegment = AutoSegment::create( source, target, Flags::Horizontal );
        if (not globalSegment->isGlobal()) globalSegment->setFlags( AutoSegment::SegLongLocal );
        cdebug_log(145,0) << "[Create global segment (8)]: " << globalSegment << endl;
      
      } else if ((east() != NULL) && (west() != NULL) ) {
        cdebug_log(145,0) << "Case EW" <<  endl;
        AutoContact* target = AutoContactTurn::create( _gcell, _net, Session::getContactLayer(1) );
        source->setX(_gcell->getXMin() + _gcell->getWidth()/2);
        source->setY(east()->getComponent()->getY());
        target->setX(_gcell->getXMin() + _gcell->getWidth()/2);
        target->setY(west()->getComponent()->getY());
        cdebug_log(145,0) << "1" <<  endl;
        
        if (east()->getComponent()->getY() > west()->getComponent()->getY()){
          cdebug_log(145,0) << "2.1" <<  endl;
          swap( source, target );

          cdebug_log(145,0) << "3.1" <<  endl;
          if ( east() and (_fromHook != east()) ){
            push( east(), target );
          } else if ( west() and (_fromHook != west()) ){
            push( west(), source );
            targetContact = target;
          } else {
            cerr << Warning( "Something is wrong with the globals and the fromHook in this Strut. 3"
                           , getString(_gcell).c_str() )
                 << endl;
          }
        } else {
          cdebug_log(145,0) << "2.2" <<  endl;
          if ( east() and (_fromHook != east()) ){ 
            push( east(), source );
            targetContact = target;
          } else if ( west() and (_fromHook != west()) ){
            push( west(), target );
          } else {
            cerr << Warning( "Something is wrong with the globals and the fromHook in this Strut. 4"
                           , getString(_gcell).c_str() )
                 << endl;
          }
        }
        cdebug_log(145,0) << "[Create AutoContact]: " << source << endl;
        cdebug_log(145,0) << "[Create AutoContact]: " << target << endl;

        AutoSegment* globalSegment = AutoSegment::create( source, target, Flags::Vertical );
        if (not globalSegment->isGlobal()) globalSegment->setFlags( AutoSegment::SegLongLocal );
        cdebug_log(145,0) << "[Create global segment (9)]: " << globalSegment << endl;

      } else {
        cerr << Warning( "Something is wrong with the globals in this Strut. 5 "
                       , getString(_gcell).c_str() )
             << endl;
      }
    } else if ( getNumberGlobals() == 3 ){
      cdebug_log(145,0) << "Case 3 globals: " <<  getNumberGlobals() << endl;

      AutoContact* turn = AutoContactTurn::create( _gcell, _net, Session::getContactLayer(1) );
      AutoContact* xtee = NULL;
    //xtee = AutoContactHTee::create( _gcell, _net, Session::getContactLayer(2) );

      if        ((north() != NULL) && (south() != NULL) && (east() != NULL)){ 
        cdebug_log(145,0) << "Case NSE " << endl;
        xtee = AutoContactHTee::create( _gcell, _net, Session::getContactLayer(1) );
        if (north()->getComponent()->getX() < south()->getComponent()->getX()){
          turn->setX(north()->getComponent()->getX());
          xtee->setX(south()->getComponent()->getX());
          if ( north() and (_fromHook != north()) ) push( north(), turn );
          else                                      targetContact = turn;
          if ( south() and (_fromHook != south()) ) push( south(), xtee );
          else                                      targetContact = xtee;

        } else {
          turn->setX(south()->getComponent()->getX());
          xtee->setX(north()->getComponent()->getX());
          if ( north() and (_fromHook != north()) ) push( north(), xtee );
          else                                      targetContact = xtee;
          if ( south() and (_fromHook != south()) ) push( south(), turn );
          else                                      targetContact = turn;
        }
        turn->setY(east()->getComponent()->getY());
        xtee->setY(east()->getComponent()->getY());
        if ( east() and (_fromHook != east()) ) push( east(), xtee );
        else                                    targetContact = xtee;
     
        cdebug_log(145,0) << "[Create AutoContact]: " << turn << endl;
        cdebug_log(145,0) << "[Create AutoContact]: " << xtee << endl;
        AutoSegment* globalSegment = AutoSegment::create( turn, xtee, Flags::Horizontal );
        if (not globalSegment->isGlobal()) globalSegment->setFlags( AutoSegment::SegLongLocal );
        cdebug_log(145,0) << "[Create global segment (10)]: " << globalSegment << endl;

      } else if ((north() != NULL) && (south() != NULL) && (west() != NULL)){ 
        cdebug_log(145,0) << "Case NSW " << endl;
        xtee = AutoContactHTee::create( _gcell, _net, Session::getContactLayer(1) );

        if (north()->getComponent()->getX() < south()->getComponent()->getX()){
          xtee->setX(north()->getComponent()->getX());
          turn->setX(south()->getComponent()->getX());
          if ( north() and (_fromHook != north()) ) push( north(), xtee );
          else                                      targetContact = xtee;
          if ( south() and (_fromHook != south()) ) push( south(), turn );
          else                                      targetContact = turn;

        } else {
          xtee->setX(south()->getComponent()->getX());
          turn->setX(north()->getComponent()->getX());
          if ( north() and (_fromHook != north()) ) push( north(), turn );
          else                                      targetContact = turn;
          if ( south() and (_fromHook != south()) ) push( south(), xtee );
          else                                      targetContact = xtee;
        }
        turn->setY(west()->getComponent()->getY());
        xtee->setY(west()->getComponent()->getY());
        if ( west() and (_fromHook != west()) ) push( west(), xtee );
        else                                    targetContact = xtee;
     
        cdebug_log(145,0) << "[Create AutoContact]: " << xtee << endl;
        cdebug_log(145,0) << "[Create AutoContact]: " << turn << endl;
        AutoSegment* globalSegment = AutoSegment::create( xtee, turn, Flags::Horizontal );
        if (not globalSegment->isGlobal()) globalSegment->setFlags( AutoSegment::SegLongLocal );
        cdebug_log(145,0) << "[Create global segment (11)]: " << globalSegment << endl;


      } else if ((east() != NULL)  && (north() != NULL) && (west() != NULL)){
        cdebug_log(145,0) << "Case EWN " << endl;
        xtee = AutoContactVTee::create( _gcell, _net, Session::getContactLayer(2) );

        if (east()->getComponent()->getY() < west()->getComponent()->getY()){
          turn->setY(east()->getComponent()->getY());
          xtee->setY(west()->getComponent()->getY());
          if ( east() and (_fromHook != east()) ) push( east(), turn );
          else                                    targetContact = turn;
          if ( west() and (_fromHook != west()) ) push( west(), xtee );
          else                                    targetContact = xtee;

        } else {
          turn->setY(west()->getComponent()->getY());
          xtee->setY(east()->getComponent()->getY());
          if ( east() and (_fromHook != east()) ) push( east(), xtee );
          else                                    targetContact = xtee;
          if ( west() and (_fromHook != west()) ) push( west(), turn );
          else                                    targetContact = turn;
        }
        turn->setX(north()->getComponent()->getX());
        xtee->setX(north()->getComponent()->getX());
        if ( north() and (_fromHook != north()) ) push( north(), xtee );
        else                                      targetContact = xtee;
     
        cdebug_log(145,0) << "[Create AutoContact]: " << turn << endl;
        cdebug_log(145,0) << "[Create AutoContact]: " << xtee << endl;
        AutoSegment* globalSegment = AutoSegment::create( turn, xtee, Flags::Vertical );
        if (not globalSegment->isGlobal()) globalSegment->setFlags( AutoSegment::SegLongLocal );
        cdebug_log(145,0) << "[Create global segment (12)]: " << globalSegment << endl;

      } else if ((east() != NULL) && (south() != NULL) && (west() != NULL)){
        cdebug_log(145,0) << "Case EWS " << endl;
        xtee = AutoContactVTee::create( _gcell, _net, Session::getContactLayer(2) );

        if (east()->getComponent()->getY() < west()->getComponent()->getY()){
          xtee->setY(east()->getComponent()->getY());
          turn->setY(west()->getComponent()->getY());
          if ( east() and (_fromHook != east()) ) push( east(), xtee );
          else                                    targetContact = xtee;
          if ( west() and (_fromHook != west()) ) push( west(), turn );
          else                                    targetContact = turn;

        } else {
          xtee->setY(west()->getComponent()->getY());
          turn->setY(east()->getComponent()->getY());
          if ( east() and (_fromHook != east()) ) push( east(), turn );
          else                                    targetContact = turn;
          if ( west() and (_fromHook != west()) ) push( west(), xtee );
          else                                    targetContact = xtee;
        }
        turn->setX(south()->getComponent()->getX());
        xtee->setX(south()->getComponent()->getX());
        if ( south() and (_fromHook != south()) ) push( south(), xtee );
        else                                      targetContact = xtee;
     
        cdebug_log(145,0) << "[Create AutoContact]: " << xtee << endl;
        cdebug_log(145,0) << "[Create AutoContact]: " << turn << endl;
        AutoSegment* globalSegment = AutoSegment::create( xtee, turn, Flags::Vertical );
        if (not globalSegment->isGlobal()) globalSegment->setFlags( AutoSegment::SegLongLocal );
        cdebug_log(145,0) << "[Create global segment (13)]: " << globalSegment << endl;

      } else {
        cerr << Warning( "Something is wrong with the globals in this Strut. 6"
                       , getString(_gcell).c_str() )
             << endl;
      }
    } else if ( getNumberGlobals() == 4 ){
      cdebug_log(145,0) << "Case 4 globals: " <<  getNumberGlobals() << endl;
      AutoContact* turn  = AutoContactTurn::create( _gcell, _net, Session::getContactLayer(1) );
      AutoContact* hteeh = AutoContactHTee::create( _gcell, _net, Session::getContactLayer(2) );
      AutoContact* vteev = AutoContactVTee::create( _gcell, _net, Session::getContactLayer(1) );

      if (  (north()->getComponent()->getX() < south()->getComponent()->getX() )
         && (east()->getComponent ()->getY() < west()->getComponent ()->getY() )
         ) { 
        cdebug_log(145,0) << "(N.X < S.X) & (E.Y < W.Y)" <<  endl;
        turn->setX (north()->getComponent()->getX());
        turn->setY (east()->getComponent ()->getY());
        hteeh->setX(south()->getComponent()->getX());
        hteeh->setY(east()->getComponent ()->getY());
        vteev->setX(north()->getComponent()->getX());
        vteev->setY(west()->getComponent ()->getY());

        if ( east() and (_fromHook != east()) ) push( east(), hteeh );
        else targetContact = hteeh;
        if ( west() and (_fromHook != west()) ) push( west(), vteev );
        else targetContact = vteev;
        if ( north() and (_fromHook != north()) ) push( north(), vteev );
        else targetContact = vteev;
        if ( south() and (_fromHook != south()) ) push( south(), hteeh );
        else targetContact = hteeh;

        cdebug_log(145,0) << "[Create AutoContact]: " << hteeh << endl;
        cdebug_log(145,0) << "[Create AutoContact]: " << vteev << endl;
        cdebug_log(145,0) << "[Create AutoContact]: " << turn << endl;
        AutoSegment* globalSegment1 = AutoSegment::create( turn, hteeh, Flags::Horizontal );
        AutoSegment* globalSegment2 = AutoSegment::create( turn, hteeh, Flags::Vertical );
        cdebug_log(145,0) << "[Create global segment (14.1)]: " << globalSegment1 << endl;
        cdebug_log(145,0) << "[Create global segment (14.2)]: " << globalSegment2 << endl;

      } else if (  (north()->getComponent()->getX() > south()->getComponent()->getX() )
                && (east()->getComponent ()->getY() < west()->getComponent ()->getY() )
                ) {
        cdebug_log(145,0) << "(N.X > S.X) & (E.Y < W.Y)" <<  endl;
        turn->setX (south()->getComponent()->getX());
        turn->setY (west()->getComponent ()->getY());
        hteeh->setX(north()->getComponent()->getX());
        hteeh->setY(east()->getComponent ()->getY());
        vteev->setX(south()->getComponent()->getX());
        vteev->setY(east()->getComponent ()->getY());

        if ( east() and (_fromHook != east()) ) push( east(), hteeh );
        else targetContact = hteeh;
        if ( west() and (_fromHook != west()) ) push( west(), turn );
        else targetContact = turn;
        if ( north() and (_fromHook != north()) ) push( north(), hteeh );
        else targetContact = hteeh;
        if ( south() and (_fromHook != south()) ) push( south(), vteev );
        else targetContact = vteev;

        cdebug_log(145,0) << "[Create AutoContact]: " << hteeh << endl;
        cdebug_log(145,0) << "[Create AutoContact]: " << vteev << endl;
        cdebug_log(145,0) << "[Create AutoContact]: " << turn << endl;
        AutoSegment* globalSegment1 = AutoSegment::create( vteev, hteeh, Flags::Horizontal );
        AutoSegment* globalSegment2 = AutoSegment::create( vteev, turn, Flags::Vertical );
        cdebug_log(145,0) << "[Create global segment (15.1)]: " << globalSegment1 << endl;
        cdebug_log(145,0) << "[Create global segment (15.2)]: " << globalSegment2 << endl;

      } else if (  (north()->getComponent()->getX() < south()->getComponent()->getX() )
                && (east()->getComponent ()->getY() > west()->getComponent ()->getY() )
                ) {
        cdebug_log(145,0) << "(N.X < S.X) & (E.Y > W.Y)" <<  endl;
        turn->setX (north()->getComponent()->getX());
        turn->setY (east()->getComponent ()->getY());
        hteeh->setX(south()->getComponent()->getX());
        hteeh->setY(east()->getComponent ()->getY());
        vteev->setX(south()->getComponent()->getX());
        vteev->setY(west()->getComponent ()->getY());

        if ( east() and (_fromHook != east()) ) push( east(), hteeh );
        else targetContact = hteeh;
        if ( west() and (_fromHook != west()) ) push( west(), vteev );
        else targetContact = vteev;
        if ( north() and (_fromHook != north()) ) push( north(), turn );
        else targetContact = turn;
        if ( south() and (_fromHook != south()) ) push( south(), vteev );
        else targetContact = vteev;

        cdebug_log(145,0) << "[Create AutoContact]: " << hteeh << endl;
        cdebug_log(145,0) << "[Create AutoContact]: " << vteev << endl;
        cdebug_log(145,0) << "[Create AutoContact]: " << turn << endl;
        AutoSegment* globalSegment1 = AutoSegment::create( turn, hteeh, Flags::Horizontal );
        AutoSegment* globalSegment2 = AutoSegment::create( vteev, hteeh, Flags::Vertical );
        cdebug_log(145,0) << "[Create global segment (16.1)]: " << globalSegment1 << endl;
        cdebug_log(145,0) << "[Create global segment (16.2)]: " << globalSegment2 << endl;

      } else {
        cdebug_log(145,0) << "(N.X > S.X) & (E.Y > W.Y)" <<  endl;
        turn->setX (south()->getComponent()->getX());
        turn->setY (east()->getComponent ()->getY());
        hteeh->setX(north()->getComponent()->getX());
        hteeh->setY(east()->getComponent ()->getY());
        vteev->setX(south()->getComponent()->getX());
        vteev->setY(west()->getComponent ()->getY());

        if ( east() and (_fromHook != east()) ) push( east(), hteeh );
        else targetContact = hteeh;
        if ( west() and (_fromHook != west()) ) push( west(), vteev );
        else targetContact = vteev;
        if ( north() and (_fromHook != north()) ) push( north(), hteeh );
        else targetContact = hteeh;
        if ( south() and (_fromHook != south()) ) push( south(), vteev );
        else targetContact = vteev;

        cdebug_log(145,0) << "[Create AutoContact]: " << hteeh << endl;
        cdebug_log(145,0) << "[Create AutoContact]: " << vteev << endl;
        cdebug_log(145,0) << "[Create AutoContact]: " << turn << endl;
        AutoSegment* globalSegment1 = AutoSegment::create( turn, hteeh, Flags::Horizontal );
        AutoSegment* globalSegment2 = AutoSegment::create( vteev, turn, Flags::Vertical );
        cdebug_log(145,0) << "[Create global segment (17.1)]: " << globalSegment1 << endl;
        cdebug_log(145,0) << "[Create global segment (17.2)]: " << globalSegment2 << endl;
      }
    } else { 
      cerr << Warning( "Something is wrong with the number of globals in this Strut. 7"
                     , getString(_gcell).c_str() )
           << endl;
    }
    cdebug_log(145,0) << "doStrut done" << endl;
    cdebug_tabw(145,-1);

    return targetContact;
  }


  void  GCellTopology::_doIoPad ()
  {
    throw Error( "GCellTopology::_doIoPad() Unimplemented, blame goes to J.-P. Chaput." );
  }


} // Anonymous namespace.




namespace Anabatic {

  using Hurricane::Name;
  using Hurricane::DebugSession;
  using Hurricane::Error;
  using Hurricane::Warning;
  using Hurricane::Bug;
  using CRL::addMeasure;
  using CRL::getMeasure;


  void  AnabaticEngine::_loadGrByNet ()
  {
    cmess1 << "  o  Building detailed routing from global." << endl;
  //cmess1 << Dots::asDouble("     - Saturation",getMeasure<double>(getCell(),"Sat.")->getData()) << endl;

    startMeasures();
    openSession();

    for ( Net* net : getCell()->getNets() ) {
      if (NetRoutingExtension::isAutomaticGlobalRoute(net)) {
        DebugSession::open( net, 144, 160 );
        AutoSegment::setAnalogMode( NetRoutingExtension::isAnalog(net) );
        _loadNetGlobalRouting( net );
        Session::revalidate();
        DebugSession::close();
      }
    }
    AutoSegment::setAnalogMode( false );

#if defined(CHECK_DATABASE)
    _check ( "after Anabatic loading" );
#endif

    Session::close();

    stopMeasures();
    printMeasures( "load" );

    addMeasure<size_t>( getCell(), "Globals", AutoSegment::getGlobalsCount() );
    addMeasure<size_t>( getCell(), "Edges"  , AutoSegment::getAllocateds() );
  }


  void  AnabaticEngine::_loadNetGlobalRouting ( Net* net )
  {
  //DebugSession::open( 145, 150 );

    cdebug_log(149,0) << "Anabatic::_loadNetGlobalRouting( " << net << " )" << endl;
    cdebug_tabw(145,1);

    ForkStack    forks;
    Hook*        sourceHook    = NULL;
    AutoContact* sourceContact = NULL;

    lookupClear();

    RoutingPads routingPads = net->getRoutingPads();
    size_t      degree      = routingPads.getSize();

    if (degree == 0) {
      cmess2 << Warning("Net \"%s\" do not have any RoutingPad (ignored)."
                       ,getString(net->getName()).c_str()) << endl;
      cdebug_tabw(145,-1);
      return;
    }
    if (degree < 2) {
#if 0
      if (not getDemoMode())
        cmess2 << Warning("Net \"%s\" have less than 2 plugs/pins (ignored)."
                         ,getString(net->getName()).c_str()) << endl;
#endif
      cdebug_tabw(145,-1);
      return;
    }

    cdebug_tabw(145,1);
    Hook*  startHook    = NULL;
    GCell* lowestGCell  = NULL;
    size_t unconnecteds = 0;
    size_t connecteds   = 0;

    GCellTopology::init( degree );

    cdebug_log(145,0) << "Start RoutingPad Ring" << endl;
    forEach ( RoutingPad*, startRp, routingPads ) {
      bool segmentFound = false;

      cdebug_log(145,0) << "startRp  : " << startRp << endl;
      cdebug_log(145,0) << "startHook: " << startHook << endl;
      forEach ( Hook*, ihook, startRp->getBodyHook()->getHooks() ) {
        cdebug_log(145,0) << "Component " << ihook->getComponent() << endl;
        Segment* segment = dynamic_cast<Segment*>( ihook->getComponent() );

        if (segment) {
          ++connecteds;
          segmentFound = true;

          GCellTopology  gcellConf ( this, forks, *ihook, NULL );
          if (gcellConf.getStateG() == 1) {
            if ( (lowestGCell == NULL) or (*gcellConf.getGCell() < *lowestGCell) ) {
              cdebug_log(145,0) << "Potential starting GCell " << gcellConf.getGCell() << endl;
              lowestGCell = gcellConf.getGCell();
              startHook   = *ihook;
            }
            break;
          }
        }
      }

      unconnecteds += (segmentFound) ? 0 : 1;
      if ( (unconnecteds > 10) and (connecteds == 0) ) {
        cerr << Warning("More than 10 unconnected RoutingPads (%u) on %s, missing global routing?"
                       ,unconnecteds, getString(net->getName()).c_str() ) << endl;

        NetRoutingExtension::create( net )->setFlags  ( NetRoutingState::Excluded );
        NetRoutingExtension::create( net )->unsetFlags( NetRoutingState::AutomaticGlobalRoute );
        cdebug_tabw(145,-1);
        return;
      }
    // Uncomment the next line to disable the lowest GCell search.
    // (takes first GCell with exactly one global).
    //if (startHook) break;
    }
    cdebug_tabw(145,-1);

    if (startHook == NULL) { singleGCell( this, net ); cdebug_tabw(145,-1); return; }

    GCellTopology  startGCellConf ( this, forks, startHook, NULL );
    cdebug_log(145,0) << endl;
    cdebug_log(145,0) << "--------~~~~=={o}==~~~~--------" << endl;
    cdebug_log(145,0) << endl;
    cdebug_log(145,0) << "Start building from:" << startGCellConf.getGCell() << endl;
    startGCellConf.construct();

    sourceHook    = forks.getFrom   ();
    sourceContact = forks.getContact();
    forks.pop();

    while ( sourceHook ) {
      GCellTopology  gcellConf ( this, forks, sourceHook, sourceContact );
      gcellConf.construct();

      sourceHook    = forks.getFrom();
      sourceContact = forks.getContact();
      forks.pop();

      cdebug_log(145,0) << "Popping (from) " << sourceHook << endl;
      cdebug_log(145,0) << "Popping (to)   " << sourceContact << endl;
    }

    lookupClear();
    Session::revalidate();
  //Breakpoint::stop( 0, "After construct" );
    
#if THIS_IS_DISABLED
    set<AutoSegment*>::iterator iover = overconstraineds.begin();
    for ( ; iover != overconstraineds.end() ; ++iover ) {
      (*iover)->makeDogLeg( (*iover)->getAutoSource()->getGCell(), true );
    }
#endif

    Session::revalidate();
    GCellTopology::fixSegments();
    cdebug_tabw(145,-1);

  //DebugSession::close();
  }


} // Anabatic namespace.
