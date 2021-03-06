# -*- mode:Python -*-

import sys
import os.path
import string
import traceback
import Hurricane
from   Hurricane import DbU
from   Hurricane import DataBase
from   Hurricane import Technology
from   Hurricane import Layer
from   Hurricane import BasicLayer
from   Hurricane import DiffusionLayer
from   Hurricane import TransistorLayer
from   Hurricane import RegularLayer
from   Hurricane import ContactLayer
from   Hurricane import ViaLayer
from   CRL       import AllianceFramework
from   helpers   import ErrorMessage


technologyFile = '<No technology file specified>'


class CompositeLayerType ( object ):

    Regular     = 1
    Diffusion   = 2
    Transistor  = 3
    Contact     = 4
    Via         = 5

    def __init__ ( self, code ):
        self._code = code
        return

    def __int__ ( self ):
        return self._code

    def __str__ ( self ):
        if self._code == CompositeLayerType.Regular:    return 'TypeRegular'
        if self._code == CompositeLayerType.Diffusion:  return 'TypeDiffusion'
        if self._code == CompositeLayerType.Transistor: return 'TypeTransistor'
        if self._code == CompositeLayerType.Contact:    return 'TypeContact'
        if self._code == CompositeLayerType.Via:        return 'TypeVia'
        return 'TypeUnknown (%d)', self._code

    def __repr__ ( self ):
        return str(self)

    def realLayerLength ( self ):
        if self._code == CompositeLayerType.Regular:    return 1
        if self._code == CompositeLayerType.Diffusion:  return 3
        if self._code == CompositeLayerType.Transistor: return 4
        if self._code == CompositeLayerType.Contact:    return 5
        if self._code == CompositeLayerType.Via:        return 3
        return 0


TypeRegular     = CompositeLayerType(CompositeLayerType.Regular)
TypeDiffusion   = CompositeLayerType(CompositeLayerType.Diffusion)
TypeTransistor  = CompositeLayerType(CompositeLayerType.Transistor)
TypeContact     = CompositeLayerType(CompositeLayerType.Contact)
TypeVia         = CompositeLayerType(CompositeLayerType.Via)


class LayersLUT ( object ):

    Real         = 0x1
    Composite    = 0x2
    MissingError = 0x8

    def __init__ ( self ):
        self._realLayers      = {}
        self._compositeLayers = {}
        return

    def add ( self, layer ):
        if isinstance(layer,BasicLayer): self._realLayers     [ layer.getName() ] = layer
        else:                            self._compositeLayers[ layer.getName() ] = layer
        return

    def lookup ( self, name, flags=Real|Composite ):
        layer = None
        if flags & LayersLUT.Real and self._realLayers.has_key(name):
            layer = self._realLayers[name]
        if flags & LayersLUT.Composite and self._compositeLayers.has_key(name):
            layer = self._compositeLayers[name]

        if not layer and flags&LayersLUT.MissingError:
            raise ErrorMessage(1,['Layer <%s> is not defined (yet?).'%name])

        return layer


layersLUT = LayersLUT()


def loadRealLayers ( realLayersTable, confFile ):
    global technologyFile
    technologyFile = confFile
    technology     = DataBase.getDB().getTechnology()

    entryNo = 0
    for entry in realLayersTable:
        entryNo += 1

        try:
            if len(entry) < 2:
                raise ErrorMessage(1,['Malformed entry in <realLayersTable>.'
                                     ,'Less than two fields: missing name and/or material.'
                                     ,str(entry)
                                     ])
            if len(entry) > 2:
                if entry[1] != BasicLayer.Material.blockage:
                    raise ErrorMessage(1,['Invalid entry in <realLayersTable>.'
                                         ,'Only blockage material can have a third field.'
                                         ,str(entry)
                                         ])
                routingLayer = technology.getBasicLayer(entry[2])
                if not routingLayer:
                    raise ErrorMessage(1,['Incoherency in <realLayersTable> entry at.'
                                         ,'The metal <%s> associated to the blockage doesn\'t exist (yet?).' % entry[2]
                                         ,str(entry)
                                         ])

            basicLayer = BasicLayer.create( technology
                                          , entry[0]
                                          , BasicLayer.Material(entry[1]) )
            layersLUT.add ( basicLayer )
            if len(entry) > 2:
                routingLayer.setBlockageLayer(basicLayer)

        except Exception, e:
            ErrorMessage.wrapPrint(e,'In %s:<realLayersTable> at entry %d.' % (technologyFile,entryNo))
    return


def loadCompositeLayers ( compositeLayersData, confFile ):
    global technologyFile
    technologyFile = confFile
    technology     = DataBase.getDB().getTechnology()

    entryNo = 0
    for entry in compositeLayersData:
        entryNo += 1

        try:
            if len(entry) != 3:
                raise ErrorMessage(1,['Malformed entry in <compositeLayersTable>.'
                                     ,'Must contains exactly three fields: ( name, type, (real_layers,) ).'
                                     ,str(entry)
                                     ])
            name, layerType, realLayers = entry

            if not isinstance(layerType,CompositeLayerType):
                raise ErrorMessage(1,['Invalid entry in <compositeLayersTable>.'
                                     ,'The layer type code is not valid, should be any of:'
                                     ,'    * TypeRegular'
                                     ,'    * TypeDiffusion'
                                     ,'    * TypeTransistor'
                                     ,'    * TypeContact'
                                     ,'    * TypeVia'
                                     ,str(entry)
                                     ])
            if layerType.realLayerLength() != len(realLayers):
                raise ErrorMessage(1,['Invalid entry in <compositeLayersTable>.'
                                     ,'Layer of type <%s> contains %d real layers instead of %d.' \
                                          % (layerType,len(realLayers),layerType.realLayerLength())
                                     ,str(entry)
                                     ])

            realLayersArgs = []
            for layerName in realLayers:
                if layerName:
                    realLayersArgs += [ layersLUT.lookup(layerName
                                                        ,LayersLUT.Real
                                                        |LayersLUT.Composite
                                                        |LayersLUT.MissingError) ]
                else:
                    realLayersArgs += [ None ]
    
            compositeLayer = None
    
            if layerType == TypeRegular:
                compositeLayer = RegularLayer.create(technology,entry[0])
                compositeLayer.setBasicLayer( *realLayersArgs )
            elif layerType == TypeDiffusion:
                compositeLayer = DiffusionLayer.create(technology ,entry[0], *realLayersArgs)
            elif layerType == TypeTransistor:
                compositeLayer = TransistorLayer.create(technology ,entry[0], *realLayersArgs)
            elif layerType == TypeContact:
                compositeLayer = ContactLayer.create(technology ,entry[0], *realLayersArgs)
            elif layerType == TypeVia:
                compositeLayer = ViaLayer.create(technology ,entry[0], *realLayersArgs)
    
            layersLUT.add( compositeLayer )

        except Exception, e:
            ErrorMessage.wrapPrint(e,'In %s:<compositeLayersTable> at entry %d.' % (technologyFile,entryNo))
    return


def loadLayersExtensions ( layersExtensionsTable, confFile ):
    global technologyFile
    technologyFile = confFile
    technology     = DataBase.getDB().getTechnology()

    entryNo = 0
    for rule in layersExtensionsTable:
        entryNo += 1

        try:
            if len(rule) != 2:
                raise ErrorMessage(1,['Malformed entry in <layersExtensionsTable>.'
                                     ,'Must contains exactly two fields: ( rule_path, value ).'
                                     ,str(rule)
                                     ])
            if not     isinstance(rule[1],int  ) \
               and not isinstance(rule[1],long ) \
               and not isinstance(rule[1],float) \
               and not isinstance(rule[1],tuple):
                raise ErrorMessage(1,['Invalid entry in <layersExtensionsTable>.'
                                     ,'Rule value must be of integer, float, or pair a of those type.'
                                     ,str(rule)
                                     ])

            elements = rule[0].split('.')
            if len(elements) == 2:
              ruleLayer = layersLUT.lookup( elements[0], LayersLUT.Real|LayersLUT.Composite|LayersLUT.MissingError )
              subLayer  = None
            elif len(elements) == 3 or len(elements) == 4:
              ruleLayer = layersLUT.lookup( elements[0], LayersLUT.Composite|LayersLUT.MissingError  )
              subLayer  = layersLUT.lookup( elements[1], LayersLUT.Real )
            else:
                raise ErrorMessage(1,['Invalid entry in <layersExtensionsTable>.'
                                     ,'Rule name must contains two or three components:'
                                     ,' * \"COMP_LAYER.category.dimension\".'
                                     ,' * \"REAL_LAYER.dimension\".'
                                     ,str(rule)
                                     ])

            if isinstance(rule[1],tuple): value = ( rule[1][0], rule[1][1] )
            else:                         value =   rule[1]

            if subLayer: ruleTag  = string.join(elements[2:],'.')
            else:        ruleTag  = string.join(elements[1:],'.')

            if   ruleTag == 'minimalSpacing':  ruleLayer.setMinimalSpacing( value )
            elif ruleTag == 'extention.cap':   ruleLayer.setExtentionCap  ( subLayer, value )
            elif ruleTag == 'extention.width': ruleLayer.setExtentionWidth( subLayer, value )
            elif ruleTag == 'minimum.width':   ruleLayer.setMinimalSize   ( value )
            elif ruleTag == 'minimum.side':    ruleLayer.setMinimalSize   ( value )
            elif ruleTag == 'enclosure':
              if isinstance(value,tuple):
                ruleLayer.setEnclosure( subLayer, value[0], Layer.EnclosureH )
                ruleLayer.setEnclosure( subLayer, value[1], Layer.EnclosureV )
              else:
                ruleLayer.setEnclosure( subLayer, value, Layer.EnclosureH|Layer.EnclosureV )
            else: 
                raise ErrorMessage(1,['Invalid entry in <layersExtensionsTable>.'
                                     ,'Unknown rule kind: \".%s\", should be any of:' % ruleTag
                                     ,'    * "RULE_HEAD.minimalSpacing"'
                                     ,'    * "RULE_HEAD.extention.cap"'
                                     ,'    * "RULE_HEAD.extention.width"'
                                     ,'    * "RULE_HEAD.enclosure"'
                                     ,'    * "RULE_HEAD.minimum.width"'
                                     ,'    * "RULE_HEAD.minimum.side"'
                                     ,str(rule)
                                     ])

        except Exception, e:
            ErrorMessage.wrapPrint(e,'In %s:<layersExtensionsTable> at entry %d.' % (technologyFile,entryNo))
    return


def tagSymbolicLayers ( symbolicLayersTable, confFile ):
    global technologyFile
    technologyFile = confFile
    technology     = DataBase.getDB().getTechnology()

    entryNo = 0
    for layerName in symbolicLayersTable:
        entryNo += 1
        try:
           # This call is just to generate an error if the layer is non-existent.
            layersLUT.lookup(layerName,LayersLUT.Real|LayersLUT.Composite|LayersLUT.MissingError)
            technology.setSymbolicLayer(layerName)
        except Exception, e:
            ErrorMessage.wrapPrint(e,'In %s:<symbolicLayersTable> at entry %d.' % (technologyFile,entryNo))
    return


def loadGdsLayers ( realLayersTable, confFile ):
    technologyFile   = confFile
    technology = DataBase.getDB().getTechnology()

    entryNo = 0
    for entry in realLayersTable:
        entryNo += 1

        try:
            if len(entry) != 4:
                raise ErrorMessage(1,['Malformed entry in <realLayersTable>.'
                                      ,'Must have exactly four fields: (symb_name,real_name,GDSII_layer,GDSII_datatype).'
                                     ,str(entry)
                                     ])
            symbName, realName, gdsiiLayer,gdsiiDatatype = entry
            if not isinstance(gdsiiLayer,int):
                raise ErrorMessage(1,['Incoherency in <realLayersTable> entry.'
                                     ,'GDSII layer number is not of int type (%s).' \
                                          % helpers.stype(gdsiiLayer)
                                     ,str(entry)
                                     ])
            if not isinstance(gdsiiDatatype,int):
                raise ErrorMessage(1,['Incoherency in <realLayersTable> entry.'
                                     ,'GDSII layer Datatype is not of int type (%s).' \
                                          % helpers.stype(gdsiiDatatype)
                                     ,str(entry)
                                     ])

            basicLayer = technology.getBasicLayer(symbName)
            if not basicLayer:
                raise ErrorMessage(1,['Incoherency in <realLayersTable> entry.'
                                     ,'The real layer "%s" associated to the GDSII "%s" do not exists.' \
                                          % (symbName,realName)
                                     ,str(entry)
                                     ])
            basicLayer.setRealName    ( realName )
            basicLayer.setGds2Layer   ( gdsiiLayer )
            basicLayer.setGds2Datatype( gdsiiDatatype )

        except Exception, e:
            ErrorMessage.wrapPrint(e,'In %s:<gdsLayersTable> at entry %d.' % (technologyFile,entryNo))
    return


def initTechno ( technoConfig ):
    technology     = DataBase.getDB().getTechnology()
    if not technology:
      name = 'Unknown'
      if technoConfig.has_key('name'): name = technoConfig['name'] 
      technology = Technology.create( DataBase.getDB(), name )

    gridValue = 1
    gridUnit  = DbU.UnitPowerMicro
    for key in [ 'precision'
               , 'gridUnit'
               , 'gridValue'
               , 'gridsPerLambda'
               , 'symbolicGridStep'
               , 'polygonStep' ]:
      try:
        if key == 'precision':
          if technoConfig.has_key('precision'):
            precision = technoConfig['precision']
            if not isinstance(gridUnit,int):
                raise ErrorMessage(1,['In <technoConfig>, <precision> must be of type int (and not: %s).'
                                      % helpers.stype(precision)
                                     ])
            DbU.setPrecision(precision)
          else:
            raise ErrorMessage(1,'<technoConfig> has no <precision> defined.')

        elif key == 'gridUnit':
          if technoConfig.has_key(key):
            gridUnit = technoConfig[key]
            if gridUnit != DbU.UnitPowerPico  and \
               gridUnit != DbU.UnitPowerNano  and \
               gridUnit != DbU.UnitPowerMicro and \
               gridUnit != DbU.UnitPowerMilli and \
               gridUnit != DbU.UnitPowerUnity and \
               gridUnit != DbU.UnitPowerKilo:
                raise ErrorMessage(1,'In <technoConfig>, invalid DbU unit power for gridUnit, reseting to Micro.')
          else:
            raise ErrorMessage(1,'<technoConfig> has no <gridUnit> defined, assuming Micro.')

        elif key == 'gridValue':
          if technoConfig.has_key('gridValue'):
            gridValue = technoConfig['gridValue']
            if not isinstance(gridUnit,float) and not isinstance(gridUnit,int):
                raise ErrorMessage(1,['In <technoConfig>, <gridValue> must be of type float (and not: %s).'
                                      % helpers.stype(gridValue)
                                     ])
            DbU.setPhysicalsPerGrid(gridValue,gridUnit)
          else:
            raise ErrorMessage(1,'<technoConfig> has no <gridValue> defined.')
    
        elif key == 'gridsPerLambda':
          if technoConfig.has_key('gridsPerLambda'):
            gridsPerLambda = technoConfig['gridsPerLambda']
            if not isinstance(gridsPerLambda,int):
                raise ErrorMessage(1,['In <technoConfig>, <gridsPerLambda> must be of type int (and not: %s).'
                                    % helpers.stype(gridsPerLambda)
                                   ])
            DbU.setGridsPerLambda(gridsPerLambda)

        elif key == 'polygonStep':
          if technoConfig.has_key('polygonStep'):
            polygonStep = technoConfig['polygonStep']
            if not isinstance(polygonStep,float):
                raise ErrorMessage(1,['In <technoConfig>, <polygonStep> must be of type float (and not: %s).'
                                    % helpers.stype(polygonStep)
                                   ])
            DbU.setPolygonStep( DbU.fromGrid(polygonStep) )

        elif key == 'symbolicGridStep':
          if technoConfig.has_key('symbolicGridStep'):
            gridStep = technoConfig['symbolicGridStep']
            if not isinstance(gridStep,float):
                raise ErrorMessage(1,['In <technoConfig>, <symbolicGridStep> must be of type float (and not: %s).'
                                    % helpers.stype(gridStep)
                                   ])
            DbU.setSymbolicSnapGridStep( DbU.fromLambda(gridStep) )
    
      except Exception, e:
        ErrorMessage.wrapPrint(e)

    return
