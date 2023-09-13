#include "stdafx.h"
#include "version.h"

const char* s =
#include "readme.txt"
;

DECLARE_COMPONENT_VERSION(PLUGIN_NAME, FOO_COMPONENT_VERSION, s);
VALIDATE_COMPONENT_FILENAME(COMPONENT_NAME_DLL);
