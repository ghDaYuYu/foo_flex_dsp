#pragma once

#include "stdafx.h"
#include "ReplayGainResultPopup.h"

#define DSP_PROCESS_AHEAD 5

static const GUID guid_dynamicdsp_branch = { 0x1f6bd793, 0x61b2, 0x4572, { 0x87, 0xbb, 0x00, 0x10, 0x65, 0xce, 0xba, 0xa4 } };
static const GUID guid_replaygain_branch = { 0x0e2988ac, 0xacee, 0x4e75, { 0xbc, 0x92, 0xe8, 0xd9, 0x83, 0x34, 0x0d, 0xf9 } };
static const GUID guid_cfg_album_pattern = { 0x027c08f7, 0x881d, 0x401b, { 0x96, 0xa4, 0x00, 0xe0, 0x17, 0x83, 0x7a, 0x19 } };
static const GUID guid_cfg_thread_priority = { 0x72af045d, 0x4f5a, 0x4abc, { 0x88, 0x07, 0x20, 0x78, 0xa5, 0xea, 0xcc, 0x48 } };

static const int thread_priority_levels[7] = { THREAD_PRIORITY_IDLE, THREAD_PRIORITY_LOWEST, THREAD_PRIORITY_BELOW_NORMAL, THREAD_PRIORITY_NORMAL, THREAD_PRIORITY_ABOVE_NORMAL, THREAD_PRIORITY_HIGHEST, THREAD_PRIORITY_TIME_CRITICAL };

enum ScanMode {
  PER_FILE,
  SINGLE_ALBUM,
  ALBUM_BY_TAGS,
  ALBUM_BY_FOLDERS
};

struct Settings {
public:
  Settings(uint64_t priority, const pfc::string8& sortFormat) : m_priority(priority), m_sortFormat(sortFormat) {}
  uint64_t priority() const { return m_priority; }
  pfc::string8 sortFormat() const { return m_sortFormat; }
private:
  const uint64_t m_priority;
  const pfc::string8 m_sortFormat;
};

class ReplayGainScanProcess : public threaded_process_callback {
public:

  ReplayGainScanProcess(metadb_handle_list_cref items, dsp_chain_config_impl chain, ScanMode scanMode, const Settings& settings)
    : m_scanMode(scanMode), m_chain(chain), m_items(items), m_settings(settings), m_resultList() {}

  void on_init(HWND p_wnd) {}

  pfc::list_t<metadb_handle_list> createInputList();

  void run(threaded_process_status & p_status, abort_callback & p_abort);

  void on_done(HWND p_wnd, bool p_was_aborted);

  static void RunReplaygainScanner(metadb_handle_list_cref data, ScanMode scanMode);

private:
  ReplayGainResultList m_resultList;
  pfc::string8 m_failMsg;
  dsp_chain_config_impl m_chain;
  const metadb_handle_list m_items;
  Settings m_settings;
  ScanMode m_scanMode;

  double output_duration = 0;
  FILETIME start_time, end_time;
};

