# -*- mode: CMAKE explicit-buffer-name: "CMakeLists.txt<unittests>" -*-

 set(CMAKE_LEGACY_CYGWIN_WIN32 0)
 project(UNITTEST)

 cmake_minimum_required(VERSION 2.8.9)

 set(ignoreVariables "${BUILD_DOC}")

 list(INSERT CMAKE_MODULE_PATH 0 "${DESTDIR}$ENV{CORIOLIS_TOP}/share/cmake/Modules/")
 find_package(Bootstrap REQUIRED)
 setup_project_paths(CORIOLIS)
 list(INSERT CMAKE_MODULE_PATH 0 "${CRLCORE_SOURCE_DIR}/cmake_modules/")
 print_cmake_module_path()

 set_cmake_policies()
 check_distribution()
 setup_sysconfdir("${CMAKE_INSTALL_PREFIX}")
 setup_boost(program_options python regex wave)
 setup_qt()

 find_package(Libbfd)
 find_package(Libexecinfo        REQUIRED)
 find_package(PythonLibs         2 REQUIRED)
 find_package(PythonSitePackages REQUIRED)
 find_package(LEFDEF)
 find_package(VLSISAPD           REQUIRED)
 find_package(HURRICANE          REQUIRED)
 find_package(CORIOLIS           REQUIRED)
 
 add_subdirectory(src)
