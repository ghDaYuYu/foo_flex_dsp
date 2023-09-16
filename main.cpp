#include "stdafx.h"
#include "version.h"

const char* s =
#include "readme.txt"
;

#define ABOUT_HEADER "Switch DSP based on track metadata.\n\n" \
"Author: da yuyu\n" \
"Version: " FOO_COMPONENT_VERSION "\n" \
"Compiled: " __DATE__ "\n" \
"fb2k SDK: " PLUGIN_FB2K_SDK "\n\n" \
"Website: https://github.com/ghDaYuYu/foo_flex_dsp\n\n" \
"Acknowledgements:\n\n" \
"Flex DSP is a fork of Dynamic DSP.\n" \
"Thanks to popart for starting the Dynamic DSP project and Mario66 for continuing development.\n\n" \
"LICENSE:\n\n" \
"Copyright(c) 2023 da yuyu, Copyright(c) 2020 hydrogenaud.io user Mario66.\n\n" \
"Change log:\n\n" \
"v1.0:\n" \
"  Fix unrecoverable error switching to six-channel audio (exclusive mode).\n" \
"  Fix crash cancelling nested chain config dialog.\n" \
"  New option to log DSP updates.\n" \
"  Fixed not readable inscription in the settings window.\n" \
"\n\n" \
"More details: (Dynamic DSP readme.txt)\n\n" \

pfc::string8 get_about() { pfc::string8 str(ABOUT_HEADER);  str.add_string(s); return str; };

DECLARE_COMPONENT_VERSION(PLUGIN_NAME, FOO_COMPONENT_VERSION, get_about());
VALIDATE_COMPONENT_FILENAME(COMPONENT_NAME_DLL);
