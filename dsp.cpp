#include "stdafx.h"
#include "resource.h"
#include <wchar.h>
#include <functional>
#include <filesystem>
#include <mmdeviceapi.h>
#include "libPPUI/wtl-pp.h"
#include "version.h"
#include "guids.h"
//#include <endpointvolume.h>
#include <Functiondiscoverykeys_devpkey.h>
#include "helpers/DarkMode.h"


#define MAX_PRESENT_NAME_LEN 100
#define MAX_PATTERN_LEN 1024
#define DSP_DEFAULT_SEPARATOR ';'
#define DSP_DEFAULT_TF "[%track_flexdsp%]"

typedef pfc::map_t<pfc::string8, dsp_chain_config_impl *, pfc::string::comparatorCaseInsensitive> ChainsMap;

using namespace std::placeholders;

advconfig_branch_factory cfg_flex_dsp_branch("Flex DSP", guid_flex_dsp_branch, advconfig_branch::guid_branch_tools, 0);
advconfig_checkbox_factory cfg_log_enabled("Log DSP updates", guid_cfg_log_enabled, guid_flex_dsp_branch, false, 0);
advconfig_branch_factory replaygain_branch("ReplayGain Scanner", guid_replaygain_branch, guid_flex_dsp_branch, 0);
advconfig_string_factory cfg_album_pattern("Album grouping pattern", guid_cfg_album_pattern, guid_replaygain_branch, 0, "%album artist% | %date% | %album%");
advconfig_integer_factory cfg_thread_priority("Thread priority (1-7)", guid_cfg_thread_priority, guid_replaygain_branch, 0, 2, 1, 7);

static void RunDSPConfigPopup(const dsp_preset & p_data, HWND p_parent, dsp_preset_edit_callback & p_callback);

pfc::string8 ConvertWchar(const WCHAR * orig){
  size_t origsize = wcslen(orig);
  char *nstring = new char[origsize + 1];
  pfc::stringcvt::convert_wide_to_utf8(nstring, origsize + 1, orig, origsize);
  pfc::string8 retval(nstring);
  delete[] nstring;
  return retval;
}

void ConvertString8(const pfc::string8 orig, wchar_t * out, size_t max) {
  pfc::stringcvt::convert_utf8_to_wide(out, max, orig.get_ptr(), orig.length());
}

pfc::string8 GetDefaultPlaybackDevice()
{
  HRESULT hr = NULL;
  bool decibels = false;
  bool scalar = false;

  CoInitialize(NULL);
  IMMDeviceEnumerator *deviceEnumerator = NULL;
  hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_INPROC_SERVER,
    __uuidof(IMMDeviceEnumerator), (LPVOID *)&deviceEnumerator);
  IMMDevice *defaultDevice = NULL;

  hr = deviceEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &defaultDevice);
  deviceEnumerator->Release();
  deviceEnumerator = NULL;

  IPropertyStore *pProps = NULL;
  hr = defaultDevice->OpenPropertyStore(
    STGM_READ, &pProps);

  PROPVARIANT varName;
  // Initialize container for property value.
  PropVariantInit(&varName);

  // Get the endpoint's friendly-name property.
  hr = pProps->GetValue(
    PKEY_Device_DeviceDesc, &varName);

  return ConvertWchar(varName.pwszVal);
}

pfc::string8 ReplaceDevicePlaceholders(pfc::string8 pattern) {
  // Replace placeholders with output name and device
  static_api_ptr_t<output_manager> om;
  outputCoreConfig_t cfg;
  om->getCoreConfig(cfg);
  auto op = output_entry::g_find(cfg.m_output);
  std::string output_name = op->get_name();
  std::string device_name = op->get_device_name(cfg.m_device);
  auto output = pattern;
  output.replace_string("%output_type%", output_name.c_str());
  output.replace_string("%output_device_name%", device_name.c_str());
  output.replace_string("%windows_output_device_name%", GetDefaultPlaybackDevice().c_str());
  return output;
}

class MyDSP : public dsp_impl_base {
 public:
  MyDSP(){
    curLatency = 0;
    curDsp = pfc::string8("UNINITIALIZED");
  }

  MyDSP(dsp_preset const & in) {
    parse_preset(&chainsMap, &titleformat, &separator, in);
    // Remove newline characters
    t_size pos = 0;
    while (true) {
      pos = titleformat.find_first('\n', pos);
      if (pos == ~0) break;
      titleformat.remove_chars(pos, 1);
    }
    curDsp = pfc::string8("UNINITIALIZED");
  }

  ~MyDSP(){
    FreeMemory t;
    chainsMap.enumerate(std::bind(&FreeMemory::Traverse, t, _1, _2));
  }

  static GUID g_get_guid() {
    //This is our GUID. Generate your own one when reusing this code.
    static const GUID guid = { 0x6f5ef674, 0x725a, 0x408d, { 0x94, 0x5b, 0x11, 0x90, 0x6f, 0xc5, 0xcd, 0x99 } };
    return guid;
  }

  static void g_get_name(pfc::string_base & p_out) {
    p_out = "Flex DSP";
  }

  void addChain(const dsp_chain_config_impl *chain) {
    if (chain != NULL) {
      for (size_t j = 0; j < chain->get_count(); j++) {
        currentChain.add_item(chain->get_item(j));
      }
    }
  }

  void updateCurrentChainForString(const pfc::string8& str) {
    currentChain.remove_all();
    if (separator == 0) {
      addChain(chainsMap[str]);
    } else {
      pfc::list_t<pfc::string_part_ref> splits;
      pfc::splitStringSimple_toList(splits, separator, str.toString());
      for (size_t i = 0; i < splits.get_count(); i++) {
        dsp_chain_config_impl *subChain = chainsMap[pfc::string8(splits.get_item(i))];
        addChain(subChain);
      }
    }
  }

  bool on_chunk(audio_chunk * chunk, abort_callback & cb) {
    metadb_handle_ptr track;
    get_cur_file(track);
    dsp_chunk_list_impl outputChunks;

    if (track != curTrack || chunk == NULL) { // this is a new track (or end of playlist needing flush)
      pfc::string8 newDsp;
      // find out the chain name for this track
      if (track != NULL && chunk != NULL) {
        auto titleformat_tmp = ReplaceDevicePlaceholders(titleformat);
        static_api_ptr_t<titleformat_compiler>()->compile_safe(compiledTitleformat, titleformat_tmp);
        track->format_title(NULL, newDsp, compiledTitleformat, 0);
      } else {
        newDsp = pfc::string8("");
      }

      // see if chain has changed since last file
      if (curDsp != newDsp){
        // new chain wanted, first flush out all the audio data still in the old dsp chain
        if (currentChain.get_count() > 0){
          if (chunk != NULL){
            outputChunks.add_chunk(chunk);
          }
          curLatency = manager.run(&outputChunks, curTrack, FLUSH, cb);
          manager.flush();
          chunk = NULL;
        }

        //switch to new chain
        curLatency = 0;
        updateCurrentChainForString(newDsp);
        if (currentChain.get_count() > 0){
          manager.set_config(currentChain);
        }
        curDsp = newDsp;
      }
      curTrack = track;
    }

    // feed new chunk through our current selected dsp chain, if any
    if (chunk != NULL){
      if (currentChain.get_count() > 0){
        dsp_chunk_list_impl list;
        list.add_chunk(chunk);
        curLatency = manager.run(&list, curTrack, 0, cb);
        for (size_t i = 0; i < list.get_count(); i++){
          outputChunks.add_chunk(list.get_item(i));
        }
      } else {
        // no selected dsp chain, just output the input chunk unmodified
        outputChunks.add_chunk(chunk);
      }
    }

    // throw all output chunks at the framework
    // (original chunk always discarded)
    for (size_t i = 0; i < outputChunks.get_count(); i++){
      audio_chunk *newChunk = insert_chunk();
      *newChunk = *(outputChunks.get_item(i));
    }
    return false;
  }

  void on_endofplayback(abort_callback & cb) {
    // The end of playlist has been reached, we've already received the last decoded audio chunk.
    // We need to finish any pending processing and output any buffered data through insert_chunk().
    on_chunk(NULL, cb);
  }

  void on_endoftrack(abort_callback & cb) {
    // Should do nothing except for special cases where your DSP performs special operations when changing tracks.
    // If this function does anything, you must change need_track_change_mark() to return true.
    // If you have pending audio data that you wish to output, you can use insert_chunk() to do so.    
  }

  void flush() {
    // If you have any audio data buffered, you should drop it immediately and reset the DSP to a freshly initialized state.
    // Called after a seek etc.
    manager.flush();
  }

  double get_latency() {
    // If the DSP buffers some amount of audio data, it should return the duration of buffered data (in seconds) here.
    return curLatency;
  }

  bool need_track_change_mark() {
    // Return true if you need on_endoftrack() or need to accurately know which track we're currently processing
    // WARNING: If you return true, the DSP manager will fire on_endofplayback() at DSPs that are before us in the chain on track change to ensure that we get an accurate mark, so use it only when needed.
    return false;
  }

  static bool g_get_default_preset(dsp_preset & p_out) {
    if (import_preset(p_out)) {
      return true;
    }    

    ChainsMap chainsMap;
    pfc::string8 tf(DSP_DEFAULT_TF);
    make_preset(chainsMap, tf, DSP_DEFAULT_SEPARATOR, p_out);
    return true;
  }

  static void g_show_config_popup(const dsp_preset &p_data, HWND p_parent, dsp_preset_edit_callback &p_callback) {
    ::RunDSPConfigPopup(p_data, p_parent, p_callback);
  }

  static bool g_have_config_popup() {
    return true;
  }

  static void make_preset(const ChainsMap &cMap, const pfc::string8& tfString, char separator, dsp_preset &out) {
    uint32_t count = static_cast<uint32_t>(cMap.get_count());
    dsp_preset_builder builder;
    builder << tfString;
    builder << count;
    SerializeChains t;
    t.builder = &builder;
    cMap.enumerate(std::bind(&SerializeChains::Traverse, t, _1, _2));
    builder << separator;
    builder.finish(g_get_guid(), out);
  }

  static pfc::string8 genFilePath() {
    static std::string path;
    if (path.empty()) {
      path = std::string(core_api::get_profile_path()).append("\\configuration\\").append(core_api::get_my_file_name()).append(".exp").substr(7, std::string::npos);
    }
    return pfc::string8(path.c_str());
  }

  static bool import_preset(ChainsMap* cMap, pfc::string8* tfString, char* separator, dsp_preset& out/*const dsp_preset& in*/) {
    try {
      file_ptr l_file;
      std::error_code ec;
      std::filesystem::path os_file_name = std::filesystem::u8path(genFilePath().c_str());

      if (std::filesystem::exists(os_file_name, ec)) {
        filesystem::g_open_read(l_file, os_file_name.u8string().c_str(), fb2k::noAbort);

        dsp_preset_impl tmpDstPresetImp;
        tmpDstPresetImp.contents_from_stream(static_cast<stream_reader*>(l_file.get_ptr()), fb2k::noAbort);


        out.set_owner(g_get_guid());
        out.set_data(tmpDstPresetImp.get_data(), tmpDstPresetImp.get_data_size());
        l_file.release();

        std::filesystem::path os_file_name_bak = os_file_name;
        os_file_name_bak.replace_extension("exp.bak");
        
        if (std::filesystem::exists(os_file_name_bak)) {
          std::filesystem::remove(os_file_name_bak, ec);
        }
        std::filesystem::rename(os_file_name, os_file_name_bak, ec);

        return true;
      }
    }
    catch (exception_io_data) {
    }
    return false;
  }

  static void export_preset(const ChainsMap& cMap, const pfc::string8& tfString, char separator, dsp_preset& out) {

    file_ptr l_file;
    std::filesystem::path os_file_name = std::filesystem::u8path(genFilePath().c_str());
    filesystem::g_open_write_new(l_file, os_file_name.u8string().c_str(), fb2k::noAbort);

    uint32_t count = static_cast<uint32_t>(cMap.get_count());
    stream_writer_formatter<> pbuilder(*(static_cast<stream_writer*>(l_file.get_ptr())), fb2k::noAbort);
    out.set_data(nullptr, 0);

    dsp_preset_builder builder;
    builder << tfString;
    builder << count;

    SerializeChains t;
    t.builder = &builder;
    cMap.enumerate(std::bind(&SerializeChains::Traverse, t, _1, _2));

    builder << separator;
    builder.finish(g_get_guid(), out);
    out.contents_to_stream(static_cast<stream_writer*>(l_file.get_ptr()), fb2k::noAbort);

    l_file.get_ptr()->flushFileBuffers(fb2k::noAbort);
    l_file.release();
  }

  static void parse_preset(ChainsMap *cMap, pfc::string8 *tfString, char* separator, const dsp_preset &in) {
    try {
      uint32_t count = 0;
      dsp_preset_parser parser(in);
      parser >> (*tfString);
      parser >> count;
      for (size_t i = 0; i < count; i++) {
        pfc::string8 g;
        parser >> g;
        dsp_chain_config_impl impl;
        parser >> impl;
        dsp_chain_config_impl* implref = new dsp_chain_config_impl(impl);
        cMap->set(g, implref);
      }
      if (parser.get_remaining() > 0) {
        parser >> (*separator);
      } else {
        (*separator) = DSP_DEFAULT_SEPARATOR;
      }
    } catch (exception_io_data) {
    }
  }

  // For chainsMap
  struct SerializeChains {
   public:
    dsp_preset_builder *builder;
    void Traverse(const pfc::string8 &key, dsp_chain_config_impl *value) {
      (*builder) << key;
      (*builder) << *value;
    }
  };

  // For chainsMap
  struct FreeMemory {
   public:
    void Traverse(const pfc::string8 &key, dsp_chain_config_impl *value) {
      delete value;
    }
  };

 private:
  //part of the preset
  pfc::string8 titleformat;
  ChainsMap chainsMap;
  char separator;
  //not part of the preset
  service_ptr_t<titleformat_object> compiledTitleformat;
  pfc::string8 curDsp;
  dsp_chain_config_impl currentChain;
  dsp_manager manager;
  double curLatency;
  metadb_handle_ptr curTrack;
};

// Formats a given title format script depending on the current playback by calling playback_control::playback_format_title
struct query_titleformat_task : main_thread_callback
{
 public:
  virtual void callback_run() {
    titleformat_object::ptr script;
    pfc::string8 pattern_tmp = ReplaceDevicePlaceholders(pattern);
    static_api_ptr_t<titleformat_compiler>()->compile_safe_ex(script, pattern_tmp);
    static_api_ptr_t<playback_control> playback_control;
    pfc::string8 returnVal;
    if (playback_control->playback_format_title(NULL, returnVal, script, NULL, playback_control::display_level_all)) {
      onSuccess(returnVal);
    } else {
      onFailure();
    }
  }

  void setPattern(const pfc::string8& pat) {
    pattern = pat;
  }

  void setOnSuccess(std::function<void(pfc::string8)> f) {
    onSuccess = f;
  }

  void setOnFailure(std::function<void()> f) {
    onFailure = f;
  }

 private:
  pfc::string8 pattern;
  std::function<void(pfc::string8)> onSuccess;
  std::function<void()> onFailure;
};

// Use dsp_factory_nopreset_t<> instead of dsp_factory_t<> if your DSP does not provide preset/configuration functionality.
static dsp_factory_t<MyDSP> g_MyDSP_factory;

class CMyDSPPopup : public CDialogImpl < CMyDSPPopup > {
 public:
  CMyDSPPopup(const dsp_preset & initData, dsp_preset_edit_callback & callback) : m_initData(initData), m_callback(callback) {}

  enum { IDD = IDD_PERTRACKDSP };

  BEGIN_MSG_MAP(CMyDSPPopup)
    MSG_WM_INITDIALOG(OnInitDialog)
    COMMAND_HANDLER_EX(IDOK, BN_CLICKED, OnOKButton)
    COMMAND_HANDLER_EX(IDCANCEL, BN_CLICKED, OnCancelButton)
    COMMAND_HANDLER_EX(IDC_CHAINSLIST, LBN_DBLCLK, OnConfigureDSP)
    COMMAND_HANDLER_EX(IDC_ADDCHAINBUTTON, BN_CLICKED, OnAddDSP)
    COMMAND_HANDLER_EX(IDC_COPYBUTTON, BN_CLICKED, OnCopyDSP)
    COMMAND_HANDLER_EX(IDC_REMOVECHAINBUTTON, BN_CLICKED, OnRemoveDSP)
    COMMAND_HANDLER_EX(IDC_NEWCHAINEDIT, EN_CHANGE, OnNewChanEditChanged)
    COMMAND_HANDLER_EX(IDC_CHAINSLIST, LBN_SELCHANGE, OnChainsListSelchanged)
    COMMAND_HANDLER_EX(IDC_REFRESHPREVIEWBUTTON, BN_CLICKED, OnRefreshPreview)
  END_MSG_MAP()

 private:

  static void setCEditString8(CEdit* cedit, pfc::string8 text) {
    WCHAR str[MAX_PATTERN_LEN];
    ConvertString8(text, str, MAX_PATTERN_LEN - 1);
    if (::IsWindow(cedit->m_hWnd)) {
      cedit->SetWindowTextW(str);
    }
  }

  void updatePreview() {
    WCHAR str[MAX_PATTERN_LEN];
    pfc::string8 pattern;
    titleformatEdit.GetWindowTextW(str, MAX_PATTERN_LEN - 1);
    pattern = ConvertWchar(str);

    service_ptr_t<query_titleformat_task> cb = new service_impl_t<query_titleformat_task>();
    cb->setPattern(pattern);
    cb->setOnSuccess(std::bind(&setCEditString8, &previewEdit, _1));
    cb->setOnFailure(std::bind(&setCEditString8, &previewEdit, "n/a"));

    static_api_ptr_t<main_thread_callback_manager> cb_manager;
    cb_manager->add_callback(cb);
  }

  BOOL OnInitDialog(CWindow, LPARAM) {
    m_hparent = ::GetActiveWindow();
    chainsList = GetDlgItem(IDC_CHAINSLIST);
    newChainEdit = GetDlgItem(IDC_NEWCHAINEDIT);
    titleformatEdit = GetDlgItem(IDC_TITLEFORMATEDIT);
    previewEdit = GetDlgItem(IDC_PREVIEW);
    checkMulti = GetDlgItem(IDC_CHECKMulti);
    MyDSP::parse_preset(&chainsMap, &tfString, &separator , m_initData);
    checkMulti.ToggleCheck(separator == ';');
    WCHAR str[MAX_PATTERN_LEN];
    ConvertString8(tfString, str, MAX_PATTERN_LEN - 1);
    titleformatEdit.SetWindowTextW(str);
    FillChainsListBox t;
    t.chainsList = &chainsList;
    chainsMap.enumerate(std::bind(&FillChainsListBox::Traverse, t, _1, _2));
    updatePreview();
    m_dark.AddDialogWithControls(*this);
    return TRUE;
  }

  struct FillChainsListBox {
   public:
    CListBox *chainsList;
    void Traverse(const pfc::string8& key, dsp_chain_config_impl *value) {
      wchar_t str[MAX_PRESENT_NAME_LEN];
      ConvertString8(key, str, MAX_PRESENT_NAME_LEN - 1);
      chainsList->AddString(str);
    }
  };

  void OnOKButton(UINT, int id, CWindow) {
    WCHAR str[MAX_PATTERN_LEN];
    titleformatEdit.GetWindowTextW(str, MAX_PATTERN_LEN - 1);
    tfString = ConvertWchar(str);
    separator = checkMulti.IsChecked() ? ';' : 0;

    dsp_preset_impl preset;
    MyDSP::make_preset(chainsMap, tfString, separator, preset);
    m_callback.on_preset_changed(preset);

    if (uButton_GetCheck(m_hWnd, IDC_CHECK_EXPORT_SETUP)) {
      MyDSP::export_preset(chainsMap, tfString, separator, preset);
    }
    
    MyDSP::FreeMemory t;
    chainsMap.enumerate(std::bind(&MyDSP::FreeMemory::Traverse, t, _1, _2));
    EndDialog(id);
  }

  void OnCancelButton(UINT, int id, CWindow) {
    MyDSP::FreeMemory t;
    chainsMap.enumerate(std::bind(&MyDSP::FreeMemory::Traverse, t, _1, _2));
    EndDialog(id);
  }

  void OnRefreshPreview(UINT, int id, CWindow) {
    updatePreview();
  }

  void OnAddDSP(UINT, int id, CWindow) {
    WCHAR str[MAX_PRESENT_NAME_LEN];
    newChainEdit.GetWindowTextW(str, MAX_PRESENT_NAME_LEN - 1);
    if (wcslen(str) > 0){
      newChainEdit.SetWindowTextW(L"");
      chainsList.AddString(str);
      chainsMap.set(ConvertWchar(str), new dsp_chain_config_impl());
    }
  }

  void OnCopyDSP(UINT, int id, CWindow) {
    int selectedItem = chainsList.GetCurSel();
    if (selectedItem == LB_ERR) return; 

    WCHAR strNewKey[MAX_PRESENT_NAME_LEN];
    newChainEdit.GetWindowTextW(strNewKey, MAX_PRESENT_NAME_LEN - 1);
    if (wcslen(strNewKey) == 0) return;

    WCHAR strCopyFromKey[MAX_PRESENT_NAME_LEN];
    chainsList.GetText(selectedItem, strCopyFromKey);
    pfc::string8 copyFromKey = ConvertWchar(strCopyFromKey);
    if (!chainsMap.exists(copyFromKey)) return;

    pfc::string8 newKey = ConvertWchar(strNewKey);
    newChainEdit.SetWindowTextW(L"");
    chainsList.AddString(strNewKey);
    dsp_chain_config_impl* newChain = new dsp_chain_config_impl();
    newChain->copy(*(chainsMap[copyFromKey]));
    chainsMap.set(newKey, newChain);
  }

  void OnRemoveDSP(UINT, int id, CWindow) {
    int selectedItem = chainsList.GetCurSel();
    if (selectedItem != LB_ERR){
      WCHAR str[MAX_PRESENT_NAME_LEN];
      chainsList.GetText(selectedItem, str);
      chainsList.DeleteString(selectedItem);
      delete chainsMap[ConvertWchar(str)];
      chainsMap.remove(ConvertWchar(str));
    }
  }

  void OnNewChanEditChanged(UINT, int id, CWindow) {
    WCHAR str[MAX_PRESENT_NAME_LEN];
    newChainEdit.GetWindowTextW(str, MAX_PRESENT_NAME_LEN - 1);
    CButton addBtn = GetDlgItem(IDC_ADDCHAINBUTTON);
    CButton copyBtn = GetDlgItem(IDC_COPYBUTTON);
    bool enabled = wcslen(str) != 0 && !chainsMap.exists(ConvertWchar(str));
    addBtn.EnableWindow(enabled);
    copyBtn.EnableWindow(enabled && chainsList.GetCurSel() != LB_ERR);
  }

  void OnChainsListSelchanged(UINT, int id, CWindow) {
    CButton copyBtn = GetDlgItem(IDC_COPYBUTTON);
    CButton removeBtn = GetDlgItem(IDC_REMOVECHAINBUTTON);
    removeBtn.EnableWindow(chainsList.GetCurSel() != LB_ERR);
    WCHAR str[MAX_PRESENT_NAME_LEN];
    newChainEdit.GetWindowTextW(str, MAX_PRESENT_NAME_LEN - 1);
    copyBtn.EnableWindow(wcslen(str) != 0 && !chainsMap.exists(ConvertWchar(str)) && chainsList.GetCurSel() != LB_ERR);
  }

  void OnConfigureDSP(UINT itemNum, int id, CWindow wnd) {
    int selectedItem = chainsList.GetCurSel();
    if (selectedItem != LB_ERR) {
      WCHAR str[MAX_PRESENT_NAME_LEN];
      chainsList.GetText(selectedItem, str);
      pfc::string8 name = ConvertWchar(str);
      // Call dsp configuration popup in main thread
      /* m_mthelper.add(this, name); */
      // Call dsp configuration in current thread (bugfix for fb 1.6)
      dsp_chain_config_impl *chain = chainsMap[ConvertWchar(str)];
      static_api_ptr_t<dsp_config_manager>().get_ptr()->configure_popup(*chain, m_hparent, name.toString());
    }
  }

  const dsp_preset & m_initData; // modal dialog so we can reference this caller-owned object.
  dsp_preset_edit_callback & m_callback;

  CListBox chainsList;
  CCheckBox checkMulti;
  CEdit newChainEdit;
  CEdit titleformatEdit;
  CEdit previewEdit;
  fb2k::CDarkModeHooks m_dark;

  pfc::string8 tfString;
  ChainsMap chainsMap;
  char separator;

  HWND m_hparent = nullptr;
  //callInMainThreadHelper m_mthelper;

public:
  
  /*
  void inMainThread(const pfc::string8& chainName) {
    dsp_chain_config_impl *chain = chainsMap[chainName];
    // Set this->m_hWnd as parent, otherwise the user will be able to close this window
    // but leave the DSP popup open => crash
    static_api_ptr_t<dsp_config_manager>().get_ptr()->configure_popup(*chain, this->m_hWnd, chainName.toString());
  }

  friend class callInMainThread;
  */

};

static void RunDSPConfigPopup(const dsp_preset & p_data, HWND p_parent, dsp_preset_edit_callback & p_callback) {
  CMyDSPPopup popup(p_data, p_callback);
  if (popup.DoModal(p_parent) != IDOK) p_callback.on_preset_changed(p_data);
}
