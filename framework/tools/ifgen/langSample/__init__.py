#
# Init file for the Sample language package
#
# Copyright (C) Sierra Wireless Inc. Use of this work is subject to license.
#


import commandLib
import codeTypes

import interfaceParser

#
# Return the language specific implementation of the command library.
# Also do any language specific initialization.
#
def GetCommandLib():
    # Specify the codeTypes library for the interface parser to use.
    interfaceParser.SetCodeTypeLibrary(codeTypes)

    # Return the C specific command library
    return commandLib

