#include "stdafx.h"
#include "ReplayGainScanner.h"

static const GUID guid_mygroup = { 0x47da0baf, 0xe51d, 0x4923, { 0xa9, 0x2a, 0x22, 0x21, 0x3f, 0x4f, 0x30, 0x70 } };

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
    case cmd_scanPerTrack: p_out = "Scan per-file track gain (plus DSP)"; break;
    case cmd_scanAlbumByTag: p_out = "Scan selection as album (by tags) (plus DSP)"; break;
    case cmd_scanAsSingleAlbum: p_out = "Scan selection as a single album (plus DSP)"; break;
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
    static const GUID guid_scanPerTrack = { 0x72c65879, 0x7fc5, 0x4170, { 0x85, 0x1c, 0x6d, 0x83, 0x34, 0xc4, 0x9f, 0x33 } };
    static const GUID guid_scanAlbumByTag = { 0x04bc319c, 0x2520, 0x47e4, { 0x9b, 0x56, 0x64, 0xda, 0xc5, 0x10, 0xb4, 0x4c } };
    static const GUID guid_scanAsSingleAlbum = { 0x5e205948, 0x7178, 0x4202, { 0x95, 0x2e, 0x2c, 0x67, 0xec, 0x72, 0xe2, 0xae } };
    switch (p_index) {
    case cmd_scanPerTrack: return guid_scanPerTrack;
    case cmd_scanAlbumByTag: return guid_scanAlbumByTag;
    case cmd_scanAsSingleAlbum: return guid_scanAsSingleAlbum;
    default: uBugCheck(); // should never happen unless somebody called us with invalid parameters - bail
    }
  }

  bool get_item_description(unsigned p_index, pfc::string_base & p_out) {
    switch (p_index) {
    case cmd_scanPerTrack: p_out = "Scan per-file track gain (plus DSP)"; break;
    case cmd_scanAlbumByTag: p_out = "Scan selection as album (by tags) (plus DSP)"; break;
    case cmd_scanAsSingleAlbum: p_out = "Scan selection as a single album (plus DSP)"; break;
    default: uBugCheck(); // should never happen unless somebody called us with invalid parameters - bail
    }
  }
};

static contextmenu_item_factory_t<MyItem> g_myitem_factory;