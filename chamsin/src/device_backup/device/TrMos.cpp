// ****************************************************************************************************
// File: TrMos.cpp
// Authors: Wu YiFei
// Date   : 21/12/2006 
// ****************************************************************************************************

#include "Instance.h"
#include "MetaTransistor.h"
#include "Net.h"
#include "Transistor.h"
#include "Transistors.h"

#include "UpdateSession.h"

#include "DtrAccess.h"

#include "TrMos.h"
using namespace Hurricane;


namespace DEVICE {

// ****************************************************************************************************
// TrMos implementation
// ****************************************************************************************************

TrMos::TrMos(Library* library, const Name& name):
    Device(library, name),
    _polarity(Transistor::N),
    _isBsConnected(false),
    _m(1),
    _sourceIsFirst(true),
    _hasDummy(false),
    _hasRing(true),
    _tr1(NULL),
    _capaRouting(0)
{}


TrMos* TrMos::create(Library* library, const Name & name) {
    TrMos* trmos= new TrMos(library, name);
    trmos->_postCreate();
    return trmos;
}

void TrMos::_postCreate() {
   Inherit::_postCreate();

   // do something.
   // Initialize pin order list and other attributes. 
   // **********************************************
  // _lowPinOrder[0]=D;
  // _lowPinOrder[1]=G;

  // _highPinOrder[0]=S;
  // _highPinOrder[1]=B;

   _highPinOrder.push_back(D);
   _highPinOrder.push_back(G);

   _lowPinOrder.push_back(S);
   _lowPinOrder.push_back(B);

   double minWidth = (DtrAccess::getDtrAccess())->getSingleRealRuleByLabel(string("RW_ALU1")); 

   _widthOfSourceWire = minWidth;
   _widthOfDrainWire = minWidth;

}

void TrMos::create(const Transistor::Polarity& polarity, const bool isbsconnected)
{
   if( _tr1 ) {
      throw Error("Can't Create Logical View of TrMos " +  getString(getName()) +
              " : " + "it has already been created"); 
   }

   _polarity = polarity;
   _isBsConnected = isbsconnected;

   // MetaTransistor is in the same library than Trmos
   // ************************************************
   Library * library = getLibrary();

   // Create signals
   // **************
   Net * drain  = NULL;
   Net * source = NULL;
   Net * grid   = NULL;
   Net * bulk   = NULL;

   (drain  = Net::create(this, Name("drain")))->setExternal(true);
   (source = Net::create(this, Name("source")))->setExternal(true);
   (grid   = Net::create(this, Name("grid")))->setExternal(true);
   
   if(!isbsconnected) { 
     (bulk   = Net::create(this, Name("bulk")))->setExternal(true);
   }


   // Instancier a MetaTransistor and create the connection
   // The name of MetaTransistor is nameoftrmos_tr1
   // ****************************************************

   _tr1 = MetaTransistor::create(library, Name( getString(getName())+"_Mos1" ), _polarity);
   Instance * instance = Instance::create(this,
           Name("Ins_" + getString(_tr1->getName())),
           _tr1);

   instance->getPlug(_tr1->getNet(Name("DRAIN")))->setNet(drain);
   instance->getPlug(_tr1->getNet(Name("SOURCE")))->setNet(source);
   instance->getPlug(_tr1->getNet(Name("GRID")))->setNet(grid);

   if(!isbsconnected)
     instance->getPlug(_tr1->getNet(Name("BULK")))->setNet(bulk);
   else 
     instance->getPlug(_tr1->getNet(Name("BULK")))->setNet(source);


}  


void TrMos::generate(const unsigned m, const bool sourceisfirst, const bool hasring
    , const unsigned nbsourcecolumn, const unsigned nbdraincolumn)
// *********************************************************************************
{
   if( !_tr1 ) {
      throw Error("Can't Create Physical View for " +  getString(this) +
              " : " + "Logical view has't been created yet.");
   }

//   if( !(_transistorList.empty()) ) {
//      throw Error("Can't Create Physical View of TrMos " +  getString(getName()) + " : " 
//	  + "it has already been created"); 
//   }

   // Check out param of realization.
   // *******************************
   if( m <= 0 ) 
     throw Error("Can't generate for " +  getString(this) + " : m "
             + getString(m) + " is invalid."); 

   if(nbsourcecolumn<1)
     throw Error("Can't generate for " + getString(this)
             + " : nbsourcecolumn " + getString(nbsourcecolumn) + " is invalid.");

   if(nbdraincolumn<1)
     throw Error("Can't generate for" + getString(this) + " : nbdraincolumn "
             + getString(nbdraincolumn) + " is invalid.");


   if(!(_transistorList.empty())) {
      _Flush();
   }

   _m = m;
   _sourceIsFirst = sourceisfirst;
   _hasRing = hasring;

   // Motifs are in the same library than Trmos
   // *****************************************
   Library * library = getLibrary();

   cout << "################################################################" << endl <<
       "####    BEGIN AUTOGENERATON FOR " + _getTypeName() + " " + getString(getName()) + " #####" << endl <<
       "################################################################" << endl << endl;

//   OpenUpdateSession();

/* (1) */   

   //IF_DEBUG_HUR_ANALOG
   cout << "***  Stage 1 : CreateLayout of " + getString(this) + " Begin ***" <<endl;
   //END_IF

   // Create Motifs according to m, and instance the Motifs according
   // to the Meta-Transistor . 
   // Set m of MetaTransistor.
   // The name of motif is nameofDevice_nameofMetaTrans_finger_index
   // ****************************************************************
   _tr1->setM(_m);

   for(unsigned i=0; i<m; i++){
     Transistor* finger = Transistor::create(library,
             getString(_tr1->getName()) + "_Finger_" + getString(i),
             _polarity);

     _transistorList.push_back(finger);
     Instance::create(_tr1, Name("Ins_" + getString(finger->getName())), finger); 
   }

   //IF_DEBUG_HUR_ANALOG

   cout << "***  Stage 1 : CreateLayout of " + getString(this) + " finish ***" <<endl;
   cout <<   getString(_tr1) + " 's M is " + getString(_tr1->getM()) + ".\n"
        <<   getString(_m) + " Transistors are created.\n" <<endl; 

   //END_IF
   
/* (2) */

   //IF_DEBUG_HUR_ANALOG
   cout << "***  Stage 2 : CreateLayout of " + getString(this) + " Begin ***" <<endl;
   //END_IF

   // Create connexion for each MetaTransistor.
   // *****************************************
   _tr1->createConnection();

   //IF_DEBUG_HUR_ANALOG
   cout << "***  Stage 2 : CreateLayout of " + getString(this) + " finish ***" <<endl;
   cout << "  The connection in " + getString(_tr1) + " is created.\n"
        <<endl;
   //END_IF
   
   // Pseudo dimensionnement of metatransistor.
   // In the futur, this will be the work of auto-dimensionnement tool (DSES).
   // ************************************************************************
//   _tr1->setLe(10);
//   _tr1->setWe(11);

/* (3) */
   //IF_DEBUG_HUR_ANALOG
   cout << "***  Stage 3 : CreateLayout of " + getString(this) + " Begin ***" <<endl;
   //END_IF
   
   // Set dessin Parameter of generation for each finger. 
   // ***************************************************
   double l_finger = _tr1->getLe() ;
   double w_finger = (_tr1->getWe()) / (double)(_tr1->getM()) ;
   unsigned count = 0;

   Transistor::MaskV1Info * masqueinfo = new Transistor::MaskV1Info(l_finger, w_finger); 
   masqueinfo->setNbSourceColumn(nbsourcecolumn);
   masqueinfo->setNbDrainColumn(nbdraincolumn);

   list<Transistor*>::iterator i = _transistorList.begin()
      , j = _transistorList.end();   

   if(_m == 1) {
      masqueinfo->setType(Transistor::Type::SINGLE);
      (*(_transistorList.begin()))->setMaskInfo(masqueinfo);
   } else if(_m%2==0) { // if m is pair, create two left fingers if is source first.
                      // and create two right fingers if is drain first.
        while(i!=j) {
         if(++count>2) 
           masqueinfo->setType(Transistor::Type::INTERNAL);
	 else {
	   if ( _sourceIsFirst )
	     masqueinfo->setType(Transistor::Type::LEFT);
	   else 
	     masqueinfo->setType(Transistor::Type::RIGHT);
	 }

	 (*i)->setMaskInfo(masqueinfo);
	 i++;
        }
   } else if(_m%2==1) { // if m is impair, create one left finger, one right finger.
        while(i!=j){
	  ++ count ;
	  if (count == 1)
	    masqueinfo-> setType(Transistor::Type::LEFT);
	  else if (count == 2)  
	    masqueinfo-> setType(Transistor::Type::RIGHT);
	  else
	    masqueinfo-> setType(Transistor::Type::INTERNAL);

          (*i)->setMaskInfo(masqueinfo);
	  i++;
	}
   }

   delete masqueinfo;
   
   //IF_DEBUG_HUR_ANALOG
   cout << "***  Stage 3 : CreateLayout of " + getString(this) + " finish ***" <<endl;
   cout << "Real l_finger is " + getString(l_finger) + "." << endl
        << "Real w_finger is " + getString(w_finger) + "." << endl
	<<endl;
   //END_IF

/* (4) */
   //IF_DEBUG_HUR_ANALOG
   cout << "***  Stage 4 : CreateLayout of " + getString(this) + " Begin ***" <<endl;
   cout << "Call GenerateLayout for " + getString(_tr1) 
         + " who will launch the generator of its fingers" << ".\n"
	<<endl;
   //END_IF

   // Call function CreateLayout of MetaTransistor to launch the generator of finger.
   // *******************************************************************************

   setTerminal(false);
//
//   IF_DEBUG_HUR_ANALOG
//   cout << endl;
//   cout << "Real l of " << (long)_tr1 << getString(_tr1) + " is " + getString(_tr1->_le) + "." << endl
//        << "Real w of " << (long)_tr1 << getString(_tr1) + " is " + getString(_tr1->_we) + "." << endl
//	<<endl;
//   END_IF
//
   _tr1->createLayout();
   
   //IF_DEBUG_HUR_ANALOG
   cout << "***  Stage 4 : CreateLayout of " + getString(this) + " finish ***"<<endl
        << endl;
   //END_IF

/* (5) */   
   // Lauch Algo of Placement and routage selected.
   // *********************************************
   /* to do */

   _PlaceAndRoute();

   cout << " Place And Route " <<endl;

   for_each_instance(instance, getInstances())
    //instance->setTransformation(instance->getTransformation());      
    instance->unmaterialize();      
    instance->materialize();      

//IF_DEBUG_HUR_ANALOG
    cout << getString(instance) <<" 's boundingBox is " << getString(instance->getBoundingBox())<<endl;
//END_IF
     
   end_for

//   CloseUpdateSession();

//IF_DEBUG_HUR_ANALOG
   cout << getString(this) << " 's primary (without wire) boundingBox is " << getString(getBoundingBox()) <<endl;
//END_IF

   materialize();

//IF_DEBUG_HUR_ANALOG
   cout << getString(this) << " 's boundingBox is " << getString(getBoundingBox()) <<endl;
//END_IF

   cout << endl
        << "################################################################" <<endl
        << "####    END AUTOGENERATON FOR " + _getTypeName() + " " + getString(getName()) + " #####" <<endl
        << "################################################################" <<endl
	<< endl;
} 


void TrMos::setLowPinOrder(const PinName pin1, const PinName pin2)
// ***************************************************************
{
   _lowPinOrder[0]=pin1;
   _lowPinOrder[1]=pin2;

   vector<PinName>::iterator i = _lowPinOrder.begin(), j = _lowPinOrder.end();

   cout << " Low Pin Order " << endl;

   while(i!=j) {
    cout << *i << endl;
    i++;
   }

}  


void TrMos::setHighPinOrder(const PinName pin1, const PinName pin2)
// *****************************************************************
{
   _highPinOrder[0]=pin1;
   _highPinOrder[1]=pin2;

   vector<PinName>::iterator i = _highPinOrder.begin(), j = _highPinOrder.end();

   cout << " High Pin Order " << endl;

   while(i!=j) {
    cout << *i << endl;
    i++;
   }
}  


void TrMos::_Flush()
// ****************
{
  if(_transistorList.empty()) {
      throw Error("Can't delete Physical View of TrMos " +  getString(getName()) + " : "  + "il doesn't exist"); 
  }

  _tr1->Flush();

  _transistorList.clear();

  // Delete all segments of TrMos
  // ****************************
  /* to do */

  
}  


string TrMos::_getString() const
// ***************************************
{
     string s= Inherit::_getString();
     return s;
}

Record* TrMos::_getRecord() const
// *********************************
{
	Record* record = Inherit::_getRecord();
	return record;
}

} // end of namespace Device 