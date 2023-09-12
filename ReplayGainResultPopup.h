#pragma once

#include "stdafx.h"
#include "resource.h"


struct ReplayGainResult {
  metadb_handle_ptr metadb;
  replaygain_result::ptr track;  // Track gain and peak
  replaygain_result::ptr album;  // Album gain and peak
  pfc::string8 errorMessage;  // If success equals false, this error message will be shown in the status column.
  bool success;  // Whether the track could be scanned successfully.
};

typedef pfc::list_t<ReplayGainResult> ReplayGainResultList;

class CReplayGainResultPopup : public CDialogImpl<CReplayGainResultPopup>, public CDialogResize<CReplayGainResultPopup> {
public:
  CReplayGainResultPopup(const ReplayGainResultList& replayGains, double sample_duration, unsigned __int64 processing_duration)
    : m_sample_duration(sample_duration), m_processing_duration(processing_duration), m_replaygains(replayGains) { }

  ~CReplayGainResultPopup() {}

  enum { IDD = IDD_SCANRESULT };

  BEGIN_MSG_MAP(CMyResultsPopup)
    MSG_WM_INITDIALOG(OnInitDialog)
    COMMAND_HANDLER_EX(IDCANCEL, BN_CLICKED, OnCancel)
    COMMAND_HANDLER_EX(IDOK, BN_CLICKED, OnAccept)
    MSG_WM_NOTIFY(OnNotify)
    CHAIN_MSG_MAP(CDialogResize<CReplayGainResultPopup>)
  END_MSG_MAP()

  BEGIN_DLGRESIZE_MAP(CMyResultsPopup)
    DLGRESIZE_CONTROL(IDC_LIST1, DLSZ_SIZE_X | DLSZ_SIZE_Y)
    DLGRESIZE_CONTROL(IDC_STATIC1, DLSZ_SIZE_X | DLSZ_MOVE_Y)
    DLGRESIZE_CONTROL(IDOK, DLSZ_MOVE_X | DLSZ_MOVE_Y)
    DLGRESIZE_CONTROL(IDCANCEL, DLSZ_MOVE_X | DLSZ_MOVE_Y)
  END_DLGRESIZE_MAP()

  static void RunResultPopup(const ReplayGainResultList& replayGains, double sample_duration, unsigned __int64 processing_duration, HWND parent);

private:

  class rg_apply_filter : public file_info_filter {
  public:
    rg_apply_filter(const pfc::map_t<metadb_handle_ptr, ReplayGainResult>& p_map) : m_results(p_map) {}
    ~rg_apply_filter() {}
    virtual bool apply_filter(metadb_handle_ptr p_location, t_filestats p_stats, file_info & p_info);
  private:
    pfc::map_t<metadb_handle_ptr, ReplayGainResult> m_results;
  };

  static inline float format_replaygain(float lu);

  BOOL OnInitDialog(CWindow, LPARAM);

  LRESULT OnNotify(int, LPNMHDR message);

  void OnAccept(UINT, int id, CWindow);

  void OnCancel(UINT, int id, CWindow);

  double m_sample_duration;
  unsigned __int64 m_processing_duration;
  ReplayGainResultList m_replaygains;
  CListViewCtrl m_listview;
  service_ptr_t<titleformat_object> m_script;
  pfc::string8_fast m_temp;
  pfc::stringcvt::string_os_from_utf8_fast m_convert;
};



