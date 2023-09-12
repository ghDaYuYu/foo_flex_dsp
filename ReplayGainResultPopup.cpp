#include "ReplayGainResultPopup.h"

bool CReplayGainResultPopup::rg_apply_filter::apply_filter(metadb_handle_ptr p_location, t_filestats p_stats, file_info & p_info) {
  const ReplayGainResult* result = NULL;
  // Search for the result of the given location.
  auto it = m_results.find(p_location);
  if (it.is_valid()) result = &(it->m_value);
  if (result != NULL) {
    replaygain_info info = p_info.get_replaygain();
    replaygain_info orig = info;

    if (result->album != NULL) {
      info.m_album_gain = result->album->get_gain();
      info.m_album_peak = result->album->get_peak();
    } else {
      info.remove_album_gain();
      info.remove_album_peak();
    }
    info.m_track_gain = result->track->get_gain();
    info.m_track_peak = result->track->get_peak();
    p_info.set_replaygain(info);
    return info != orig;
  } else {
    return false;
  }
}

float CReplayGainResultPopup::format_replaygain(float lu) {
  if (lu == std::numeric_limits<float>::quiet_NaN() || lu == std::numeric_limits<float>::infinity() || lu < -70) return replaygain_info::gain_invalid;
  else return lu;
}

BOOL CReplayGainResultPopup::OnInitDialog(CWindow, LPARAM) {
  DlgResize_Init();

  double processing_duration = (double)(m_processing_duration)* 0.0000001;
  double processing_ratio = m_sample_duration / processing_duration;

  pfc::string8_fast temp;

  temp = "Calculated in: ";
  temp += pfc::format_time_ex(processing_duration);
  temp += ", speed: ";
  temp += pfc::format_float(processing_ratio, 0, 2);
  temp += "x";

  uSetDlgItemText(m_hWnd, IDC_STATIC1, temp);

  m_listview = GetDlgItem(IDC_LIST1);
  // Explorer like item selection and highlighting style
  SetWindowTheme((HWND)m_listview, L"Explorer", NULL);
  // LVS_EX_DOUBLEBUFFER for semi-transparent selection
  m_listview.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);

  LVCOLUMN lvc = { 0 };
  lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
  lvc.fmt = LVCFMT_LEFT;
  lvc.pszText = _T("Name");
  lvc.cx = 225;
  lvc.iSubItem = 0;
  m_listview.InsertColumn(0, &lvc);
  lvc.pszText = _T("Status");
  lvc.cx = 150;
  lvc.iSubItem = 1;
  m_listview.InsertColumn(1, &lvc);
  lvc.fmt = LVCFMT_RIGHT;
  lvc.pszText = _T("Album gain");
  lvc.cx = 75;
  m_listview.InsertColumn(2, &lvc);
  lvc.pszText = _T("Track gain");
  m_listview.InsertColumn(3, &lvc);
  lvc.pszText = _T("Album peak");
  m_listview.InsertColumn(4, &lvc);
  lvc.pszText = _T("Track peak");
  m_listview.InsertColumn(5, &lvc);

  for (size_t i = 0; i < m_replaygains.get_count(); i++) {
    m_listview.InsertItem(i, LPSTR_TEXTCALLBACK);
    m_listview.SetItemText(i, 1, LPSTR_TEXTCALLBACK);
    m_listview.SetItemText(i, 2, LPSTR_TEXTCALLBACK);
    m_listview.SetItemText(i, 3, LPSTR_TEXTCALLBACK);
    m_listview.SetItemText(i, 4, LPSTR_TEXTCALLBACK);
    m_listview.SetItemText(i, 5, LPSTR_TEXTCALLBACK);
  }

  if (!static_api_ptr_t<titleformat_compiler>()->compile(m_script, "%title%"))
    m_script.release();

  ShowWindow(SW_SHOW);

  unsigned error_tracks = 0;
  unsigned total_tracks = 0;

  for (size_t i = 0; i < m_replaygains.get_count(); i++) {
    if (!m_replaygains[i].success) {
      error_tracks++;
    }
    total_tracks++;
  }

  if (error_tracks) {
    if (error_tracks == total_tracks) GetDlgItem(IDOK).EnableWindow(FALSE);
    popup_message::g_show(pfc::string_formatter() << pfc::format_int(error_tracks) << " out of " << pfc::format_int(total_tracks) << " items could not be processed.", "Dynamic DSP: ReplayGain Scanner - warning", popup_message::icon_error);
  }

  return TRUE;
}

LRESULT CReplayGainResultPopup::OnNotify(int, LPNMHDR message) {
  if (message->hwndFrom == m_listview.m_hWnd) {
    switch (message->code) {
    case LVN_GETDISPINFO:
    {
      LV_DISPINFO *pLvdi = (LV_DISPINFO *)message;
      unsigned item_number = pLvdi->item.iItem;
      const ReplayGainResult& result = m_replaygains[item_number];

      switch (pLvdi->item.iSubItem) {
      case 0:  // Name
        if (m_script.is_valid()) result.metadb->format_title(NULL, m_temp, m_script, NULL);
        else m_temp.reset();
        m_convert.convert(m_temp);
        pLvdi->item.pszText = (TCHAR *)m_convert.get_ptr();
        break;

      case 1:  // Status
        m_temp = result.success ? "Success" : result.errorMessage;
        m_convert.convert(m_temp);
        pLvdi->item.pszText = (TCHAR *)m_convert.get_ptr();
        break;

      case 2:  // Album gain
        if (result.success && result.album != NULL) {
          char gain[32];
          if (replaygain_info::g_format_gain(format_replaygain(result.album->get_gain()), gain)) m_temp = gain;
          else m_temp.reset();
        } else {
          m_temp.reset();
        }
        m_convert.convert(m_temp);
        pLvdi->item.pszText = (TCHAR *)m_convert.get_ptr();
        break;

      case 3:  // Track gain
        if (result.success) {
          char gain[32];
          if (replaygain_info::g_format_gain(format_replaygain(result.track->get_gain()), gain)) m_temp = gain;
          else m_temp.reset();
        } else {
          m_temp.reset();
        }
        m_convert.convert(m_temp);
        pLvdi->item.pszText = (TCHAR *)m_convert.get_ptr();
        break;

      case 4:  // Album peak
        if (result.success && result.album != NULL) {
          char peak[32];
          if (replaygain_info::g_format_peak(result.album->get_peak(), peak)) m_temp = peak;
          else m_temp.reset();
        } else {
          m_temp.reset();
        }
        m_convert.convert(m_temp);
        pLvdi->item.pszText = (TCHAR *)m_convert.get_ptr();
        break;

      case 5:  // Track peak
        if (result.success) {
          char peak[32];
          if (replaygain_info::g_format_peak(m_replaygains[item_number].track->get_peak(), peak)) m_temp = peak;
          else m_temp.reset();
        } else {
          m_temp.reset();
        }
        m_convert.convert(m_temp);
        pLvdi->item.pszText = (TCHAR *)m_convert.get_ptr();
        break;
      }
    }
    break;
    }
  }
  return 0;
}

void CReplayGainResultPopup::OnAccept(UINT, int id, CWindow) {
  metadb_handle_list list;
  pfc::map_t<metadb_handle_ptr, ReplayGainResult> map;
  for (unsigned i = 0; i < m_replaygains.get_count(); i++) {
    if (m_replaygains[i].success) {
      list.add_item(m_replaygains[i].metadb);
      map.set(m_replaygains[i].metadb, m_replaygains[i]);
    }
  }
  static_api_ptr_t<metadb_io_v2>()->update_info_async(list, new service_impl_t< rg_apply_filter >(map), core_api::get_main_window(), 0, 0);
  DestroyWindow();
}

void CReplayGainResultPopup::OnCancel(UINT, int id, CWindow) {
  DestroyWindow();
}

void CReplayGainResultPopup::RunResultPopup(const ReplayGainResultList& replayGains, double sample_duration, unsigned __int64 processing_duration, HWND parent) {
  CReplayGainResultPopup * popup = new CWindowAutoLifetime<ImplementModelessTracking<CReplayGainResultPopup>>(
    parent, replayGains, sample_duration, processing_duration);
}