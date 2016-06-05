// ****************************************************************************************************
// File: Commands.h
// Authors: R. Escassut
// Copyright (c) BULL S.A. 2000-2016, All Rights Reserved
// ****************************************************************************************************

#ifndef HURRICANE_COMMANDS
#define HURRICANE_COMMANDS

#include "Collection.h"

namespace Hurricane {

class Command;



// ****************************************************************************************************
// Commands declaration
// ****************************************************************************************************

typedef GenericCollection<Command*> Commands;



// ****************************************************************************************************
// CommandLocator declaration
// ****************************************************************************************************

typedef GenericLocator<Command*> CommandLocator;



// ****************************************************************************************************
// CommandFilter declaration
// ****************************************************************************************************

typedef GenericFilter<Command*> CommandFilter;



// ****************************************************************************************************
// for_each_command declaration
// ****************************************************************************************************

#define for_each_command(command, commands)\
/******************************************/\
{\
	CommandLocator _locator = commands.GetLocator();\
	while (_locator.IsValid()) {\
		Command* command = _locator.GetElement();\
		_locator.Progress();



} // End of Hurricane namespace.

#endif // HURRICANE_COMMANDS

// ****************************************************************************************************
// Copyright (c) BULL S.A. 2000-2016, All Rights Reserved
// ****************************************************************************************************
