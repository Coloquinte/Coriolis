# -*- explicit-buffer-name: "CMakeLists.txt<bora/src>" -*-

   include_directories( ${BORA_SOURCE_DIR}/src
                        ${CORIOLIS_INCLUDE_DIR} 
                        ${HURRICANE_INCLUDE_DIR} 
                        ${CONFIGURATION_INCLUDE_DIR} 
                        ${Boost_INCLUDE_DIRS}
                        ${QWT_INCLUDE_DIR}
                        ${QtX_INCLUDE_DIR}
                        ${PYTHON_INCLUDE_PATH}
                      )
                   set( includes     bora/Constants.h
                                     bora/BoxSet.h
                                     bora/NodeSets.h
                                     bora/HVSetState.h
                                     bora/ChannelRouting.h
                                     bora/SlicingNode.h
                                     bora/HVSlicingNode.h
                                     bora/HSlicingNode.h
                                     bora/VSlicingNode.h
                                     bora/DSlicingNode.h
                                     bora/RHVSlicingNode.h
                                     bora/RHSlicingNode.h
                                     bora/RVSlicingNode.h
				     bora/AnalogDistance.h
                                     bora/Pareto.h
                                     bora/BoraEngine.h

                      )
                   set( pyIncludes   bora/PySlicingNode.h
                                     bora/PyDSlicingNode.h
                                     bora/PyHSlicingNode.h
                                     bora/PyVSlicingNode.h
                                     bora/PyRHSlicingNode.h
                                     bora/PyRVSlicingNode.h
                                     bora/PyBoraEngine.h
                                     bora/PyGraphicBoraEngine.h
                      )
                   set( mocIncludes  bora/SlicingDataModel.h
                                     bora/SlicingDataWidget.h
                                     bora/SlicingPlotWidget.h
                                     bora/SlicingWidget.h
                                     bora/GraphicBoraEngine.h
		      )
                   set( cpps         BoxSet.cpp
                                     NodeSets.cpp
                                     HVSetState.cpp
                                     SlicingNode.cpp
                                     HVSlicingNode.cpp
                                     HSlicingNode.cpp
                                     VSlicingNode.cpp
                                     DSlicingNode.cpp
                                     RHVSlicingNode.cpp
                                     RHSlicingNode.cpp
                                     RVSlicingNode.cpp
                                     ChannelRouting.cpp
                                     BoraEngine.cpp
                                     Pareto.cpp
                                     SlicingDataModel.cpp
                                     SlicingDataWidget.cpp
                                     SlicingPlotWidget.cpp
                                     SlicingWidget.cpp
                                     GraphicBoraEngine.cpp
                      )
                   set( pyCpps       PySlicingNode.cpp
                                     PyDSlicingNode.cpp
                                     PyHSlicingNode.cpp
                                     PyRHSlicingNode.cpp
                                     PyRVSlicingNode.cpp
                                     PyVSlicingNode.cpp
                                     PyBoraEngine.cpp
                                     PyGraphicBoraEngine.cpp
                                     PyBora.cpp
                      )
          qtX_wrap_cpp( mocCpps      ${mocIncludes} )

                   set( depLibs      ${KATANA_LIBRARIES}
                                     ${ANABATIC_LIBRARIES}
                                     ${CORIOLIS_PYTHON_LIBRARIES}
                                     ${CORIOLIS_LIBRARIES}
                                     ${HURRICANE_PYTHON_LIBRARIES}
                                     ${HURRICANE_ANALOG_LIBRARIES}
                                     ${HURRICANE_GRAPHICAL_LIBRARIES}
                                     ${HURRICANE_LIBRARIES}
                                     ${CONFIGURATION_LIBRARY}
                                     ${CIF_LIBRARY}
                                     ${AGDS_LIBRARY}
                                     ${FLUTE_LIBRARIES}
                                     ${LEFDEF_LIBRARIES}
                                     ${OA_LIBRARIES}
                                     ${QWT_LIBRARY}
                                     ${QtX_LIBRARIES}
                                     ${Boost_LIBRARIES}
                                     ${PYTHON_LIBRARIES} -lutil
                      )

           add_library( bora         ${cpps} ${mocCpps} )
 set_target_properties( bora         PROPERTIES VERSION 1.0 SOVERSION 1 )
 target_link_libraries( bora         ${depLibs} )

     add_python_module( "${pyCpps}"
                        "${pyIncludes}"
                        "pybora;1.0;1"
                        Bora
                        "bora;${depLibs}"
                        include/coriolis2/bora
                      )

               install( TARGETS bora          DESTINATION lib${LIB_SUFFIX} )
               install( FILES ${includes}
		              ${mocIncludes}  DESTINATION include/coriolis2/bora ) 

   
