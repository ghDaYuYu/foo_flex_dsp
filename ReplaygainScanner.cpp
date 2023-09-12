#include "stdafx.h"
#include "helpers/input_helpers.h"
#include "ReplayGainResultPopup.h"
#include "ReplayGainScanner.h"

static advconfig_branch_factory dynamicdsp_branch("Dynamic DSP", guid_dynamicdsp_branch, advconfig_branch::guid_branch_tools, 0);
static advconfig_branch_factory replaygain_branch("ReplayGain Scanner", guid_replaygain_branch, guid_dynamicdsp_branch, 0);
static advconfig_string_factory cfg_album_pattern("Album grouping pattern", guid_cfg_album_pattern, guid_replaygain_branch, 0, "%album artist% | %date% | %album%");
static advconfig_integer_factory cfg_thread_priority("Thread priority (1-7)", guid_cfg_thread_priority, guid_replaygain_branch, 0, 2, 1, 7);

pfc::list_t<metadb_handle_list> ReplayGainScanProcess::createInputList() {
  if (m_scanMode == ScanMode::ALBUM_BY_TAGS) {
    metadb_handle_list input_files = m_items;
    pfc::string8_fast sort_format = m_settings.sortFormat();
    sort_format += "|%path_sort%";

    service_ptr_t<titleformat_object> script;
    if (!static_api_ptr_t<titleformat_compiler>()->compile(script, sort_format))
      script.release();

    input_files.sort_by_format(script, NULL);

    sort_format.truncate(sort_format.length() - strlen("|%path_sort%"));
    if (!static_api_ptr_t<titleformat_compiler>()->compile(script, sort_format))
      script.release();

    pfc::string8_fast current_album, temp_album;
    pfc::list_t<metadb_handle_list> nested_list;
    metadb_handle_list list;

    for (unsigned i = 0; i < input_files.get_count(); i++) {
      metadb_handle_ptr ptr = input_files[i];
      if (!ptr->format_title(NULL, temp_album, script, NULL)) temp_album.reset();
      if (stricmp_utf8(current_album, temp_album))
      {
        if (list.get_count()) nested_list.add_item(list);
        list.remove_all();
        current_album = temp_album;
      }
      list.add_item(ptr);
    }
    if (list.get_count()) nested_list.add_item(list);

    return nested_list;
  } else if (m_scanMode == ScanMode::PER_FILE) {
    pfc::list_t<metadb_handle_list> nested_list;
    for (unsigned i = 0; i < m_items.get_count(); i++) {
      metadb_handle_list list;
      list.add_item(m_items[i]);
      nested_list.add_item(list);
    }
    return nested_list;
  } else if (m_scanMode == ScanMode::SINGLE_ALBUM) {
    pfc::list_t<metadb_handle_list> nested_list;
    metadb_handle_list list;
    for (unsigned i = 0; i < m_items.get_count(); i++) {
      list.add_item(m_items[i]);
    }
    nested_list.add_item(list);
    return nested_list;
  }
}

void ReplayGainScanProcess::run(threaded_process_status & p_status, abort_callback & p_abort) {
  try {
    SetThreadPriority(GetCurrentThread(), thread_priority_levels[m_settings.priority() - 1]);
    
    GetSystemTimeAsFileTime(&start_time);

    // tell the decoders that we won't seek and that we don't want looping on formats that support looping.
    const t_uint32 decode_flags = input_flag_no_seeking | input_flag_no_looping;
    input_helper input;

    pfc::list_t<metadb_handle_list> input_files = createInputList();
    ReplayGainResultList tmpResultList;
    unsigned count = 0;

    for (size_t group_i = 0; group_i < input_files.get_count(); ++group_i) {

      const metadb_handle_list& curGroup = input_files[group_i];
      replaygain_result::ptr merged;

      for (size_t track_i = 0; track_i < curGroup.get_size(); ++track_i) {
        ReplayGainResult r;
        r.metadb = curGroup[track_i];
        r.album = NULL;
        r.track = NULL;
        try {
          p_abort.check();
          p_status.set_progress(count++, m_items.get_count());
          p_status.set_progress_secondary(0);
          p_status.set_item_path(curGroup[track_i]->get_path());
          input.open(NULL, curGroup[track_i], decode_flags, p_abort);

          double length;
          { // fetch the track length for proper dual progress display;
            file_info_impl info;
            if (curGroup[track_i]->get_info_async(info)) length = info.get_length();
            else length = 0;
          }
          output_duration += length;
          audio_chunk_impl_temporary l_chunk;
          replaygain_scanner::ptr scanner = static_api_ptr_t<replaygain_scanner_entry>()->instantiate();
          dsp_manager manager;
          manager.set_config(m_chain);
          dsp_chunk_list_impl chunk_list;
          double decoded = 0;
          // MAIN DECODE LOOP
          while (true) {
            bool valid = input.run(l_chunk, p_abort);  // get next chunk
            if (valid) chunk_list.add_chunk(&l_chunk);
            if (chunk_list.get_count() >= DSP_PROCESS_AHEAD || !valid) {
              manager.run(&chunk_list, curGroup[track_i], 0, p_abort);
              for (size_t chunk_i = 0; chunk_i < chunk_list.get_count(); chunk_i++) {
                scanner->process_chunk(*(chunk_list.get_item(chunk_i)));
                p_abort.check();
              }
              chunk_list.remove_all();
            }
            if (!valid) break;
            if (length > 0) { // don't bother for unknown length tracks
              decoded += l_chunk.get_duration();
              if (decoded > length) decoded = length;
              p_status.set_progress_secondary_float(decoded / length);
            }
            p_abort.check();
          }
          replaygain_result::ptr result = scanner->finalize();
          r.track = result;
          // For album repayGain
          if (track_i == 0) merged = result;
          else merged = merged->merge(result);
          r.success = true;
        } catch (std::exception const & e) {
          r.errorMessage = e.what();
          r.success = false;
        }
        tmpResultList.add_item(r);
      }
      // Set album repayGain if requested
      if (m_scanMode != ScanMode::PER_FILE) {
        for (size_t track_i = 0; track_i < tmpResultList.get_count(); ++track_i) {
          tmpResultList[track_i].album = merged;
        }
      }
      m_resultList.add_items(tmpResultList);
      tmpResultList.remove_all();
    }
    GetSystemTimeAsFileTime(&end_time);
  } catch (std::exception const & e) {
    m_failMsg = e.what();
  }
}

void ReplayGainScanProcess::on_done(HWND p_wnd, bool p_was_aborted) {
  if (!p_was_aborted) {
    if (!m_failMsg.is_empty()) {
      popup_message::g_complain("Dynamic DSP: ReplayGain Scanner - failure", m_failMsg);
    } else {
      DWORD high = end_time.dwHighDateTime - start_time.dwHighDateTime;
      DWORD low = end_time.dwLowDateTime - start_time.dwLowDateTime;
      if (end_time.dwLowDateTime < start_time.dwLowDateTime) high--;
      unsigned __int64 timestamp = ((unsigned __int64)(high) << 32) + low;
      CReplayGainResultPopup::RunResultPopup(m_resultList, output_duration, timestamp, core_api::get_main_window());
    }
  }
}

void ReplayGainScanProcess::RunReplaygainScanner(metadb_handle_list_cref data, ScanMode scanMode) {
  try {
    if (data.get_count() == 0) throw pfc::exception_invalid_params();
    // Query DSP chain
    dsp_chain_config_impl chain;
    bool ok = static_api_ptr_t<dsp_config_manager>().get_ptr()->configure_popup(chain, core_api::get_main_window(), "Dynamic DSP: ReplayGain Scanner");
    if (!ok) return;
    // Get setting
    pfc::string8 sortFormat;
    cfg_album_pattern.get(sortFormat);
    Settings settings(cfg_thread_priority.get(), sortFormat);
    // Create worker thread
    service_ptr_t<threaded_process_callback> cb = new service_impl_t<ReplayGainScanProcess>(data, chain, scanMode, settings);
    // Launch worker thread
    static_api_ptr_t<threaded_process>()->run_modeless(
      cb,
      threaded_process::flag_show_progress_dual | threaded_process::flag_show_item | threaded_process::flag_show_abort,
      core_api::get_main_window(),
      "Dynamic DSP: ReplayGain Scanner");
  } catch (std::exception const & e) {
    popup_message::g_complain("Could not start Dynamic DSP: ReplayGain Scanner process", e);
  }
}


