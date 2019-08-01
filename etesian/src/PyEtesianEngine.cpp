// -*- C++ -*-
//
// This file is part of the Coriolis Software.
// Copyright (c) UPMC/LIP6 2014-2018, All Rights Reserved
//
// +-----------------------------------------------------------------+ 
// |                   C O R I O L I S                               |
// |   E t e s i a n  -  A n a l y t i c   P l a c e r               |
// |                                                                 |
// |  Author      :                    Jean-Paul CHAPUT              |
// |  E-mail      :       Jean-Paul.Chaput@asim.lip6.fr              |
// | =============================================================== |
// |  C++ Module  :  "./PyEtesianEngine.cpp"                         |
// +-----------------------------------------------------------------+


#include "hurricane/isobar/PyCell.h"
#include "hurricane/viewer/PyCellViewer.h"
#include "hurricane/Cell.h"
#include "hurricane/viewer/ExceptionWidget.h"
#include "etesian/PyEtesianEngine.h"

# undef   ACCESS_OBJECT
# undef   ACCESS_CLASS
# define  ACCESS_OBJECT            _baseObject._object
# define  ACCESS_CLASS(_pyObject)  &(_pyObject->_baseObject)
#define   METHOD_HEAD(function)    GENERIC_METHOD_HEAD(EtesianEngine,etesian,function)


namespace  Etesian {

  using std::cerr;
  using std::endl;
  using std::hex;
  using std::ostringstream;
  using Hurricane::tab;
  using Hurricane::Exception;
  using Hurricane::Bug;
  using Hurricane::Error;
  using Hurricane::Warning;
  using Hurricane::ExceptionWidget;
  using Isobar::ProxyProperty;
  using Isobar::ProxyError;
  using Isobar::ConstructorError;
  using Isobar::HurricaneError;
  using Isobar::HurricaneWarning;
  using Isobar::ParseOneArg;
  using Isobar::ParseTwoArg;
  using Isobar::PyCell;
  using Isobar::PyCell_Link;
  using Isobar::PyCellViewer;
  using Isobar::PyTypeCellViewer;
  using CRL::PyToolEngine;


extern "C" {

#if defined(__PYTHON_MODULE__)


// +=================================================================+
// |          "PyEtesianEngine" Python Module Code Part              |
// +=================================================================+


  DirectVoidMethod(EtesianEngine,etesian,setDefaultAb)


  static PyObject* PyEtesianEngine_get ( PyObject*, PyObject* args )
  {
    cdebug_log(34,0) << "PyEtesianEngine_get()" << endl;

    EtesianEngine* etesian = NULL;
    
    HTRY
    PyObject* arg0;

    if (not ParseOneArg("Etesian.get", args, CELL_ARG, &arg0)) return NULL;
    etesian = EtesianEngine::get(PYCELL_O(arg0));
    HCATCH

    return PyEtesianEngine_Link(etesian);
  }


  static PyObject* PyEtesianEngine_create ( PyObject*, PyObject* args )
  {
    cdebug_log(34,0) << "PyEtesianEngine_create()" << endl;

    EtesianEngine* etesian = NULL;
    
    HTRY
    PyObject* arg0;

    if (not ParseOneArg("Etesian.create", args, CELL_ARG, &arg0)) return NULL;

    Cell* cell = PYCELL_O(arg0);
    etesian = EtesianEngine::get(cell);

    if (etesian == NULL) {
      etesian = EtesianEngine::create(cell);
    //if (cmess1.enabled())
    //  etesian->getEtesianConfiguration()->print(cell);
    } else
      cerr << Warning("%s already has a Etesian engine.",getString(cell).c_str()) << endl;
    HCATCH

    return PyEtesianEngine_Link(etesian);
  }


  static PyObject* PyEtesianEngine_setViewer ( PyEtesianEngine* self, PyObject* args )
  {
    cdebug_log(34,0) << "PyEtesianEngine_setViewer ()" << endl;

    HTRY
      METHOD_HEAD( "EtesianEngine.setViewer()" )
  
      PyObject* pyViewer = NULL;
      if (not PyArg_ParseTuple(args,"O:EtesianEngine.setViewer()",&pyViewer)) {
        PyErr_SetString( ConstructorError, "Bad parameters given to EtesianEngine.setViewer()." );
        return NULL;
      }
      if (IsPyCellViewer(pyViewer)) {
        etesian->setViewer( PYCELLVIEWER_O(pyViewer) );
      }
    HCATCH

    Py_RETURN_NONE;
  }


  static PyObject* PyEtesianEngine_selectBloat ( PyEtesianEngine* self, PyObject* args )
  {
    cdebug_log(34,0) << "PyEtesianEngine_selectBloat ()" << endl;

    HTRY
      METHOD_HEAD( "EtesianEngine.selectBloat()" )
  
      const char* profile = NULL;
      if (not PyArg_ParseTuple(args,"s:EtesianEngine.selectBloat()",&profile)) {
        PyErr_SetString( ConstructorError, "Bad parameters given to EtesianEngine.selectBloat()." );
        return NULL;
      }
      etesian->selectBloat( profile );
    HCATCH

    Py_RETURN_NONE;
  }


  static PyObject* PyEtesianEngine_place ( PyEtesianEngine* self )
  {
    cdebug_log(34,0) << "PyEtesianEngine_place()" << endl;
    HTRY
    METHOD_HEAD("EtesianEngine.place()")
    if (etesian->getViewer()) {
      if (ExceptionWidget::catchAllWrapper( std::bind(&EtesianEngine::place,etesian) )) {
        PyErr_SetString( HurricaneError, "EtesianEngine::place() has thrown an exception (C++)." );
        return NULL;
      }
    } else {
      etesian->place();
    }
    HCATCH
    Py_RETURN_NONE;
  }

  // Standart Accessors (Attributes).
  // DirectVoidMethod(EtesianEngine,etesian,runNegociate)
  // DirectGetBoolAttribute(PyEtesianEngine_getToolSuccess,getToolSuccess,PyEtesianEngine,EtesianEngine)

  // Standart Destroy (Attribute).
  DBoDestroyAttribute(PyEtesianEngine_destroy,PyEtesianEngine)


  PyMethodDef PyEtesianEngine_Methods[] =
    { { "get"               , (PyCFunction)PyEtesianEngine_get               , METH_VARARGS|METH_STATIC
                            , "Returns the Etesian engine attached to the Cell, None if there isn't." }
    , { "create"            , (PyCFunction)PyEtesianEngine_create            , METH_VARARGS|METH_STATIC
                            , "Create an Etesian engine on this cell." }
    , { "setViewer"         , (PyCFunction)PyEtesianEngine_setViewer         , METH_VARARGS
                            , "Associate a Viewer to this EtesianEngine." }
    , { "selectBloat"       , (PyCFunction)PyEtesianEngine_selectBloat       , METH_VARARGS
                            , "Select the Cell bloating profile." }
    , { "setDefaultAb"      , (PyCFunction)PyEtesianEngine_setDefaultAb      , METH_NOARGS
                            , "Compute and set the abutment box using the aspect ratio and the space margin." }
    , { "place"             , (PyCFunction)PyEtesianEngine_place             , METH_NOARGS
                            , "Run the placer (Etesian)." }
    , { "destroy"           , (PyCFunction)PyEtesianEngine_destroy           , METH_NOARGS
                            , "Destroy the associated hurricane object. The python object remains." }
    , {NULL, NULL, 0, NULL} /* sentinel */
    };


  DBoDeleteMethod(EtesianEngine)
  PyTypeObjectLinkPyType(EtesianEngine)


#else  // End of Python Module Code Part.


// +=================================================================+
// |           "PyEtesianEngine" Shared Library Code Part            |
// +=================================================================+


  // Link/Creation Method.
  PyTypeInheritedObjectDefinitions(EtesianEngine,PyToolEngine)
  DBoLinkCreateMethod(EtesianEngine)


  extern void  PyEtesianEngine_postModuleInit ()
  {
    //PyObject* constant;
    //LoadObjectConstant(PyTypeEtesianEngine.tp_dict,EtesianEngine::SlowMotion,"SlowMotion");
  }


#endif  // Shared Library Code Part.

}  // extern "C".

}  // Etesian namespace.
 
