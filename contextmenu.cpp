#include "stdafx.h"
//#include "atlframe.h"
#include "ReplayGainScanner.h"

#define DESC_SCAN_PER_FILE "Scan per-file track gain (Flex DSP)"
#define DESC_SCAN_SELECTION_ALBUM "Scan selection as album (by tags) (Flex DSP)"
#define DESC_SCAN_SELECTION_SINGLE_ALBUM "Scan selection as a single album (Flex DSP)"

static const GUID guid_mygroup = { 0x47914b03, 0x3f7, 0x4129, { 0xb1, 0x6e, 0xb3, 0xe3, 0xf0, 0x88, 0x45, 0x5 } };

// Switch to contextmenu_group_factory to embed your commands in the root menu but separated from other commands.
static contextmenu_group_factory g_mygroup(guid_mygroup, contextmenu_groups::replaygain, 0);
//static contextmenu_group_popup_factory g_mygroup(guid_mygroup, contextmenu_groups::root, "ReplayGain (DynamicDSP)", 0);

// Simple context menu item class.
class MyItem : public contextmenu_item_simple {
public:
  enum {
    cmd_scanPerTrack = 0,
    cmd_scanAlbumByTag,
    cmd_scanAsSingleAlbum,
    cmd_total
  };

  GUID get_parent() { return guid_mygroup; }

  unsigned get_num_items() { return cmd_total; }

  void get_item_name(unsigned p_index, pfc::string_base & p_out) {
    switch (p_index) {
    case cmd_scanPerTrack: p_out = DESC_SCAN_PER_FILE; break;
    case cmd_scanAlbumByTag: p_out = DESC_SCAN_SELECTION_ALBUM; break;
    case cmd_scanAsSingleAlbum: p_out = DESC_SCAN_SELECTION_SINGLE_ALBUM; break;
    default: uBugCheck(); // should never happen unless somebody called us with invalid parameters - bail
    }
  }

  void context_command(unsigned p_index, metadb_handle_list_cref p_data, const GUID& p_caller) {
    switch (p_index) {
    case cmd_scanPerTrack:
      ReplayGainScanProcess::RunReplaygainScanner(p_data, ScanMode::PER_FILE);
      break;
    case cmd_scanAlbumByTag:
      ReplayGainScanProcess::RunReplaygainScanner(p_data, ScanMode::ALBUM_BY_TAGS);
      break;
    case cmd_scanAsSingleAlbum:
      ReplayGainScanProcess::RunReplaygainScanner(p_data, ScanMode::SINGLE_ALBUM);
      break;
    default:
      uBugCheck();
    }
  }

  GUID get_item_guid(unsigned p_index) {
    static const GUID guid_scanPerTrack = { 0x9996af51, 0xce4e, 0x4cf2, { 0xa7, 0xff, 0xeb, 0xeb, 0xe, 0x3c, 0xb3, 0x6f } };
    static const GUID guid_scanAlbumByTag = { 0xb05ceaf9, 0xaf64, 0x4bcf, { 0xb9, 0xd0, 0x2e, 0xfc, 0x3e, 0x20, 0x7f, 0xa8 } };
    static const GUID guid_scanAsSingleAlbum = { 0x1f8b741c, 0x23e5, 0x4b9a, { 0xac, 0xc6, 0xb1, 0x23, 0x87, 0xb6, 0x7, 0xcc } };
    switch (p_index) {
    case cmd_scanPerTrack: return guid_scanPerTrack;
    case cmd_scanAlbumByTag: return guid_scanAlbumByTag;
    case cmd_scanAsSingleAlbum: return guid_scanAsSingleAlbum;
    default: uBugCheck(); // should never happen unless somebody called us with invalid parameters - bail
    }
  }

  bool get_item_description(unsigned p_index, pfc::string_base & p_out) {
    switch (p_index) {
    case cmd_scanPerTrack: p_out = DESC_SCAN_PER_FILE; break;
    case cmd_scanAlbumByTag: p_out = DESC_SCAN_SELECTION_ALBUM; break;
    case cmd_scanAsSingleAlbum: p_out = DESC_SCAN_SELECTION_SINGLE_ALBUM; break;
    default: uBugCheck(); // should never happen unless somebody called us with invalid parameters - bail
    }
  }
};

static contextmenu_item_factory_t<MyItem> g_myitem_factory;