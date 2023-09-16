#pragma once

#include "stdafx.h"
#include "ReplayGainResultPopup.h"

#define DSP_PROCESS_AHEAD 5

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

  double m_output_duration = 0;
  FILETIME m_start_time, m_end_time;
};

