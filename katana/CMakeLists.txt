# -*- explicit-buffer-name: "CMakeLists.txt<katana>" -*-

 set(CMAKE_LEGACY_CYGWIN_WIN32 0)
 project(KATANA)

 option(BUILD_DOC      "Build the documentation (doxygen)" OFF)
 option(CHECK_DATABASE "Run database in full check mode (very slow)" OFF)
 
 cmake_minimum_required(VERSION 2.8.9)

 set(ignoreVariables "${BUILD_DOC}")

 list(INSERT CMAKE_MODULE_PATH 0 "${DESTDIR}$ENV{CORIOLIS_TOP}/share/cmake/Modules/")
 find_package(Bootstrap REQUIRED)
 setup_project_paths(CORIOLIS)
 
 set_cmake_policies()
 setup_boost(program_options filesystem python regex)
 setup_qt()
 
 find_package(Libexecinfo        REQUIRED)
 find_package(LibXml2            REQUIRED)
 find_package(PythonLibs         2 REQUIRED)
 find_package(PythonSitePackages REQUIRED)
 find_package(LEFDEF             REQUIRED)
 find_package(FLUTE              REQUIRED)
 find_package(VLSISAPD           REQUIRED)
 find_package(HURRICANE          REQUIRED)
 find_package(CORIOLIS           REQUIRED)
 find_package(ANABATIC           REQUIRED)
 find_package(Doxygen)
 
 if(CHECK_DATABASE)
   add_definitions(-DCHECK_DATABASE)
 endif(CHECK_DATABASE)
 if(CHECK_DETERMINISM)
   add_definitions(-DCHECK_DETERMINISM)
 endif(CHECK_DETERMINISM)
 
 add_subdirectory(src)
 add_subdirectory(python)
 add_subdirectory(cmake_modules)
#add_subdirectory(doc)
