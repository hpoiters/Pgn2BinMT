// Pgn2BinMT Windows GUI, GPL-2.0-or-later. See COPYING.
#ifndef UNICODE
#define UNICODE
#endif
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "core.h"
#include <algorithm>
#include <chrono>
#include <commctrl.h>
#include <commdlg.h>
#include <fstream>
#include <memory>
#include <set>
#include <shellapi.h>
#include <shlobj.h>
#include <sstream>
#include <thread>
namespace fs = std::filesystem;
static HWND mainWindow, list, status, bar;
static HFONT font;
static std::vector<HWND> controls;
static std::vector<fs::path> sources;
static fs::path programDir, settingsFile, lastSource, lastOutput, lastRunOutput;
static std::thread job;
static std::unique_ptr<mt::Progress> progress;
static bool busy = false, closing = false;
static auto began = std::chrono::steady_clock::now();
enum {
  ADD = 101,
  FOLDER,
  HOME,
  ALL,
  NONE,
  CLEAR,
  THREADS = 120,
  PLY,
  MINIMUM,
  OUTPUT,
  CHOOSE,
  DEFAULTS,
  MAKE,
  STOP,
  OPEN,
  REPORT,
  SETTINGS,
  DEDUP,
  DEPTHLABEL = 140
};
std::wstring text(HWND h) {
  int n = GetWindowTextLengthW(h);
  std::wstring s(n + 1, L'\0');
  GetWindowTextW(h, s.data(), n + 1);
  s.resize(n);
  return s;
}
std::wstring wide(const std::string &s) {
  int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), s.size(), nullptr, 0);
  std::wstring w(n, L'\0');
  MultiByteToWideChar(CP_UTF8, 0, s.data(), s.size(), w.data(), n);
  return w;
}
HWND ctl(int id) { return GetDlgItem(mainWindow, id); }
HWND create(const wchar_t *cls, const wchar_t *label, DWORD style, int id) {
  HWND h = CreateWindowExW(cls == std::wstring(L"EDIT") ? WS_EX_CLIENTEDGE : 0,
                           cls, label, WS_CHILD | WS_VISIBLE | style, 0, 0, 1,
                           1, mainWindow, (HMENU)(INT_PTR)id,
                           GetModuleHandleW(nullptr), nullptr);
  SendMessageW(h, WM_SETFONT, (WPARAM)font, TRUE);
  controls.push_back(h);
  return h;
}
void place(int id, int x, int y, int w, int h) {
  MoveWindow(ctl(id), x, y, w, h, TRUE);
}
std::wstring ini(const wchar_t *key, const wchar_t *def) {
  wchar_t b[32768];
  GetPrivateProfileStringW(L"Pgn2BinMT", key, def, b, 32768,
                           settingsFile.c_str());
  return b;
}
void save() {
  WritePrivateProfileStringW(L"Pgn2BinMT", L"LastSource", lastSource.c_str(),
                             settingsFile.c_str());
  WritePrivateProfileStringW(
      L"Pgn2BinMT", L"Deduplicate",
      SendMessageW(ctl(DEDUP), BM_GETCHECK, 0, 0) == BST_CHECKED ? L"1" : L"0",
      settingsFile.c_str());
  for (auto item : {std::pair<int, const wchar_t *>(THREADS, L"Threads"),
                    {PLY, L"Ply"},
                    {MINIMUM, L"Minimum"}}) {
    auto s = text(ctl(item.first));
    WritePrivateProfileStringW(L"Pgn2BinMT", item.second, s.c_str(),
                               settingsFile.c_str());
  }
}
void resetProgress() {
  if (busy || !bar || !status)
    return;
  progress.reset();
  SendMessageW(bar, PBM_SETPOS, 0, 0);
  SetWindowTextW(status,
                 L"Select and check the source files, then click Build BIN.");
}
void defaults() {
  SetWindowTextW(ctl(THREADS), std::to_wstring(mt::defaultThreads()).c_str());
  SetWindowTextW(ctl(PLY), L"60");
  SetWindowTextW(ctl(MINIMUM), L"3");
  SendMessageW(ctl(DEDUP), BM_SETCHECK, BST_CHECKED, 0);
  lastOutput = programDir / L"Output";
  SetWindowTextW(ctl(OUTPUT), (lastOutput / L"Book.bin").c_str());
}
void add(fs::path p) {
  std::error_code ec;
  p = fs::weakly_canonical(p, ec);
  if (ec)
    return;
  auto ext = p.extension().wstring();
  std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);
  if (ext != L".pgn")
    return;
  for (auto &s : sources)
    if (fs::equivalent(p, s, ec))
      return;
  resetProgress();
  int index = sources.size();
  sources.push_back(p);
  LVITEMW item{};
  item.mask = LVIF_TEXT;
  item.iItem = index;
  std::wstring name = p.wstring();
  item.pszText = name.data();
  ListView_InsertItem(list, &item);
  ListView_SetCheckState(list, index, FALSE);
}
void scan(const fs::path &path) {
  std::error_code ec;
  fs::recursive_directory_iterator it(
      path, fs::directory_options::skip_permission_denied, ec),
      end;
  while (it != end) {
    auto p = it->path();
    if (it->is_directory(ec)) {
      auto name = p.filename().wstring();
      std::transform(name.begin(), name.end(), name.begin(), ::towlower);
      if (name == L"output" || name == L"uitvoer" || name == L"resultaten" ||
          name == L"results" || name.find(L".pgn2binmt-") == 0)
        it.disable_recursion_pending();
    } else if (it->is_regular_file(ec))
      add(p);
    it.increment(ec);
    if (ec)
      ec.clear();
  }
}
void selectFiles() {
  std::vector<wchar_t> b(65536);
  OPENFILENAMEW of{};
  of.lStructSize = sizeof(of);
  of.hwndOwner = mainWindow;
  of.lpstrFilter = L"PGN files\0*.pgn\0\0";
  of.lpstrFile = b.data();
  of.nMaxFile = b.size();
  of.lpstrInitialDir = lastSource.c_str();
  of.Flags =
      OFN_ALLOWMULTISELECT | OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
  if (GetOpenFileNameW(&of)) {
    fs::path first = b.data();
    auto next = b.data() + wcslen(b.data()) + 1;
    if (!*next) {
      add(first);
      lastSource = first.parent_path();
    } else {
      lastSource = first;
      while (*next) {
        add(first / next);
        next += wcslen(next) + 1;
      }
    }
    save();
  }
}
int CALLBACK browseCallback(HWND h, UINT m, LPARAM, LPARAM) {
  if (m == BFFM_INITIALIZED)
    SendMessageW(h, BFFM_SETSELECTIONW, TRUE, (LPARAM)lastSource.c_str());
  return 0;
}
void chooseFolder() {
  BROWSEINFOW b{};
  b.hwndOwner = mainWindow;
  b.lpszTitle = L"Choose a source folder; subfolders are included";
  b.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
  b.lpfn = browseCallback;
  auto pid = SHBrowseForFolderW(&b);
  if (pid) {
    wchar_t path[MAX_PATH];
    if (SHGetPathFromIDListW(pid, path)) {
      lastSource = path;
      scan(lastSource);
      save();
    }
    CoTaskMemFree(pid);
  }
}
void chooseOutput() {
  std::vector<wchar_t> b(32768);
  auto s = text(ctl(OUTPUT));
  wcsncpy(b.data(), s.c_str(), b.size() - 1);
  OPENFILENAMEW of{};
  of.lStructSize = sizeof(of);
  of.hwndOwner = mainWindow;
  of.lpstrFilter = L"PolyGlot BIN\0*.bin\0\0";
  of.lpstrFile = b.data();
  of.nMaxFile = b.size();
  of.lpstrInitialDir = lastOutput.c_str();
  of.lpstrDefExt = L"bin";
  of.Flags = OFN_EXPLORER | OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST;
  if (GetSaveFileNameW(&of)) {
    SetWindowTextW(ctl(OUTPUT), b.data());
    lastOutput = fs::path(b.data()).parent_path();
    save();
  }
}
unsigned number(int id, unsigned max) {
  auto s = text(ctl(id));
  size_t n = 0;
  unsigned long v = std::stoul(s, &n);
  if (n != s.size() || v < 1 || v > max)
    throw std::runtime_error("Invalid setting");
  return v;
}
void enable(bool on) {
  for (auto h : controls) {
    int id = GetDlgCtrlID(h);
    if (id == STOP)
      EnableWindow(h, !on);
    else if (id == OPEN || id == REPORT || id == 201 || id == 202 ||
             id == DEPTHLABEL)
      continue;
    else
      EnableWindow(h, on);
  }
}
void run() {
  try {
    mt::Config c;
    c.threads = number(THREADS, 256);
    c.ply = number(PLY, 10000);
    c.minGames = number(MINIMUM, 1000000000);
    c.deduplicate = SendMessageW(ctl(DEDUP), BM_GETCHECK, 0, 0) == BST_CHECKED;
    c.output = text(ctl(OUTPUT));
    if (c.output.empty())
      throw std::runtime_error("Choose an output file");
    for (size_t i = 0; i < sources.size(); i++)
      if (ListView_GetCheckState(list, i))
        c.inputs.push_back(sources[i]);
    if (c.inputs.empty())
      throw std::runtime_error("Check the PGN files you want to process first");
    auto baseOutput = c.output;
    c.output = mt::createRunOutput(baseOutput);
    lastRunOutput = c.output;
    lastOutput = baseOutput.parent_path();
    save();
    progress = std::make_unique<mt::Progress>();
    progress->running = true;
    busy = true;
    enable(false);
    SendMessageW(bar, PBM_SETPOS, 0, 0);
    SetWindowTextW(status, L"Reading PGN / processing games...");
    began = std::chrono::steady_clock::now();
    job = std::thread([c] { mt::build(c, *progress); });
  } catch (const std::exception &e) {
    if (!job.joinable()) {
      busy = false;
      enable(true);
      progress.reset();
    }
    MessageBoxW(mainWindow, wide(e.what()).c_str(), L"Pgn2BinMT",
                MB_OK | MB_ICONWARNING);
  }
}
void layout() {
  RECT r;
  GetClientRect(mainWindow, &r);
  int w = r.right, h = r.bottom;
  int y = 12, x = 12;
  for (auto pair : {std::pair<int, int>(ADD, 130),
                    {FOLDER, 115},
                    {HOME, 120},
                    {ALL, 90},
                    {NONE, 90},
                    {CLEAR, 90}}) {
    place(pair.first, x, y, pair.second, 30);
    x += pair.second + 8;
  }
  int bottom = h - 292;
  MoveWindow(list, 12, 54, w - 24, std::max(250, bottom - 66), TRUE);
  ListView_SetColumnWidth(list, 0, w - 48);
  y = bottom;
  place(210, 12, y, 80, 24);
  place(THREADS, 94, y, 70, 26);
  place(211, 182, y, 105, 24);
  place(PLY, 291, y, 70, 26);
  place(DEPTHLABEL, 370, y, 105, 24);
  place(212, 500, y, 200, 24);
  place(MINIMUM, 706, y, 70, 26);
  y += 38;
  place(213, 12, y, 530, 24);
  place(DEDUP, 555, y, w - 567, 24);
  y += 34;
  place(214, 12, y, 70, 24);
  place(OUTPUT, 90, y, w - 224, 28);
  place(CHOOSE, w - 122, y, 110, 28);
  y += 42;
  MoveWindow(bar, 12, y, w - 24, 20, TRUE);
  y += 28;
  MoveWindow(status, 12, y, w - 24, 67, TRUE);
  y += 74;
  x = 12;
  for (auto pair : {std::pair<int, int>(MAKE, 115),
                    {STOP, 90},
                    {SETTINGS, 110},
                    {DEFAULTS, 135},
                    {OPEN, 150},
                    {REPORT, 150}}) {
    place(pair.first, x, y, pair.second, 32);
    x += pair.second + 8;
  }
}
void tick() {
  unsigned ply = 0;
  try {
    ply = number(PLY, 10000);
  } catch (...) {
  }
  auto depth =
      L"(" + std::to_wstring(ply / 2) + (ply % 2 ? L".5" : L"") + L" moves)";
  SetWindowTextW(ctl(DEPTHLABEL), depth.c_str());
  if (!progress || !busy)
    return;
  std::string phase, error;
  uint64_t phaseDone = 0, phaseTotal = 0;
  {
    std::lock_guard<std::mutex> l(progress->mutex);
    phase = progress->phase;
    error = progress->error;
    phaseDone = progress->phaseDone;
    phaseTotal = progress->phaseTotal;
  }
  double seconds =
      std::chrono::duration<double>(std::chrono::steady_clock::now() - began)
          .count();
  bool reading = phase == "Reading PGN / processing games";
  bool deduplicating = phase.rfind("Deduplicating complete games", 0) == 0;
  unsigned percent = phaseTotal
                         ? std::min<uint64_t>(100, phaseDone * 100 / phaseTotal)
                     : reading && progress->totalBytes
                         ? std::min<uint64_t>(100, progress->bytes * 100 /
                                                       progress->totalBytes)
                         : 0;
  bool indeterminate = progress->running && !reading && !phaseTotal;
  auto style = GetWindowLongPtrW(bar, GWL_STYLE);
  if (indeterminate)
    style |= PBS_MARQUEE;
  else
    style &= ~PBS_MARQUEE;
  SetWindowLongPtrW(bar, GWL_STYLE, style);
  SendMessageW(bar, PBM_SETMARQUEE, indeterminate, 40);
  if (!indeterminate)
    SendMessageW(bar, PBM_SETPOS, percent, 0);
  double rateSeconds = reading ? seconds : progress->readMilliseconds / 1000.0;
  std::wostringstream s;
  s << L"Status: " << wide(phase);
  if (indeterminate)
    s << L" | Working...";
  else if (phase.rfind("Process ended:", 0) != 0)
    s << (reading ? L" | Input read: " : L" | Phase progress: ") << percent
      << L"%";
  s << L" | Games: " << progress->read << L" (total not known in advance)\r\n"
    << L"Valid: " << progress->valid << L" | Rejected: " << progress->invalid
    << L" | Duplicates: " << progress->duplicates << L" | Read rate: "
    << (uint64_t)(progress->valid / std::max(.01, rateSeconds))
    << L" games/s | PGN workers: "
    << (reading ? std::to_wstring(progress->active.load()) : L"finished")
    << L" | Dedup workers: "
    << (deduplicating ? std::to_wstring(progress->dedupActive.load()) : L"-")
    << L"\r\nRAM: " << mt::residentBytes() / 1048576
    << L" MiB | Collected records: " << progress->events << L" | BIN records: "
    << progress->records;
  if (!error.empty() && error != "Stopped")
    s << L"\r\nError: " << wide(error);
  SetWindowTextW(status, s.str().c_str());
  if (busy && !progress->running) {
    job.join();
    busy = false;
    enable(true);
    if (closing) {
      DestroyWindow(mainWindow);
      return;
    }
    if (error.empty()) {
      auto message =
          L"Process ended: OK\r\n\r\nOutput: " + lastRunOutput.wstring();
      MessageBoxW(mainWindow, message.c_str(), L"Pgn2BinMT",
                  MB_OK | MB_ICONINFORMATION);
    } else if (error == "Stopped") {
      MessageBoxW(mainWindow,
                  L"Process ended: Stopped\r\n\r\nNo new BIN file was created.",
                  L"Pgn2BinMT", MB_OK | MB_ICONINFORMATION);
    } else {
      auto message = L"Process ended: Failed\r\n\r\n" + wide(error);
      MessageBoxW(mainWindow, message.c_str(), L"Pgn2BinMT",
                  MB_OK | MB_ICONERROR);
    }
  }
}
LRESULT CALLBACK wnd(HWND h, UINT m, WPARAM w, LPARAM l) {
  switch (m) {
  case WM_GETMINMAXINFO: {
    auto p = (MINMAXINFO *)l;
    p->ptMinTrackSize = {880, 730};
    return 0;
  }
  case WM_SIZE:
    if (list)
      layout();
    return 0;
  case WM_TIMER:
    tick();
    return 0;
  case WM_NOTIFY: {
    auto *header = reinterpret_cast<NMHDR *>(l);
    if (header->hwndFrom == list && header->code == LVN_ITEMCHANGED) {
      auto *item = reinterpret_cast<NMLISTVIEW *>(l);
      if ((item->uChanged & LVIF_STATE) &&
          ((item->uOldState ^ item->uNewState) & LVIS_STATEIMAGEMASK))
        resetProgress();
    }
    break;
  }
  case WM_COMMAND:
    if (HIWORD(w))
      break;
    switch (LOWORD(w)) {
    case ADD:
      selectFiles();
      break;
    case FOLDER:
      chooseFolder();
      break;
    case HOME:
      scan(programDir);
      break;
    case ALL:
    case NONE:
      for (int i = 0; i < ListView_GetItemCount(list); i++)
        ListView_SetCheckState(list, i, LOWORD(w) == ALL);
      break;
    case CLEAR:
      resetProgress();
      sources.clear();
      ListView_DeleteAllItems(list);
      break;
    case CHOOSE:
      chooseOutput();
      break;
    case DEDUP:
      resetProgress();
      break;
    case DEFAULTS:
      resetProgress();
      defaults();
      break;
    case SETTINGS:
      SetFocus(ctl(THREADS));
      SendMessageW(ctl(THREADS), EM_SETSEL, 0, -1);
      break;
    case MAKE:
      run();
      break;
    case STOP:
      if (progress) {
        progress->stop = true;
        progress->set("Stopping... please wait");
        EnableWindow(ctl(STOP), FALSE);
      }
      break;
    case OPEN: {
      auto p =
          (lastRunOutput.empty() ? fs::path(text(ctl(OUTPUT))) : lastRunOutput)
              .parent_path();
      ShellExecuteW(h, L"open", p.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
      break;
    }
    case REPORT: {
      auto p =
          (lastRunOutput.empty() ? fs::path(text(ctl(OUTPUT))) : lastRunOutput)
              .parent_path() /
          L"Pgn2BinMT_Report.txt";
      auto incomplete = p.parent_path() / L"Pgn2BinMT_Incomplete.txt";
      if (fs::exists(incomplete))
        p = incomplete;
      if (fs::exists(p))
        ShellExecuteW(h, L"open", p.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
      else
        MessageBoxW(h, L"No final report is available yet.", L"Pgn2BinMT",
                    MB_OK);
      break;
    }
    }
    return 0;
  case WM_CLOSE:
    if (busy) {
      closing = true;
      progress->stop = true;
      progress->set("Stopping... please wait");
      EnableWindow(ctl(STOP), FALSE);
    } else
      DestroyWindow(h);
    return 0;
  case WM_DESTROY:
    save();
    PostQuitMessage(0);
    return 0;
  }
  return DefWindowProcW(h, m, w, l);
}
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
  CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  InitCommonControls();
  wchar_t exe[32768];
  GetModuleFileNameW(nullptr, exe, 32768);
  programDir = fs::path(exe).parent_path();
  wchar_t app[MAX_PATH];
  if (SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, app) != S_OK)
    return 1;
  auto settingsDir = fs::path(app) / L"Pgn2BinMT";
  std::error_code ec;
  fs::create_directories(settingsDir, ec);
  settingsFile = settingsDir / L"settings.ini";
  lastSource = ini(L"LastSource", programDir.c_str());
  lastOutput = programDir / L"Output";
  font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
  WNDCLASSW wc{};
  wc.lpfnWndProc = wnd;
  wc.hInstance = instance;
  wc.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(1));
  wc.lpszClassName = L"Pgn2BinMTWindow";
  wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
  RegisterClassW(&wc);
  mainWindow =
      CreateWindowW(wc.lpszClassName, L"Pgn2BinMT 2.0 - PGN to PolyGlot BIN",
                    WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1020,
                    800, nullptr, nullptr, instance, nullptr);
  for (auto pair : {std::pair<int, const wchar_t *>(ADD, L"Add PGN"),
                    {FOLDER, L"Browse folder"},
                    {HOME, L"Program folder"},
                    {ALL, L"Check all"},
                    {NONE, L"Uncheck all"},
                    {CLEAR, L"Clear"},
                    {CHOOSE, L"Choose file"},
                    {DEFAULTS, L"Defaults"},
                    {MAKE, L"Build BIN"},
                    {STOP, L"Stop"},
                    {OPEN, L"Open output folder"},
                    {REPORT, L"View report"},
                    {SETTINGS, L"Settings"}})
    create(L"BUTTON", pair.second, WS_TABSTOP, pair.first);
  list = create(WC_LISTVIEWW, L"", LVS_REPORT | LVS_SHOWSELALWAYS | WS_TABSTOP,
                200);
  ListView_SetExtendedListViewStyle(
      list, LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
  LVCOLUMNW col{};
  col.mask = LVCF_TEXT | LVCF_WIDTH;
  col.pszText =
      (LPWSTR)L"Source PGN files (check the files you want to process)";
  col.cx = 900;
  ListView_InsertColumn(list, 0, &col);
  for (auto pair : {std::pair<int, const wchar_t *>(210, L"Threads:"),
                    {211, L"Depth (ply):"},
                    {212, L"Minimum games per move:"},
                    {213, L"Weight: PolyGlot standard     |     RAM: "
                          L"automatic     |     Mainline only"},
                    {214, L"Output:"},
                    {DEPTHLABEL, L"(30 moves)"}})
    create(L"STATIC", pair.second, 0, pair.first);
  for (int id : {THREADS, PLY, MINIMUM})
    create(L"EDIT", L"", ES_NUMBER | ES_AUTOHSCROLL | WS_TABSTOP, id);
  create(L"EDIT", (lastOutput / L"Book.bin").c_str(),
         ES_AUTOHSCROLL | WS_TABSTOP, OUTPUT);
  create(L"BUTTON", L"Remove duplicate games", BS_AUTOCHECKBOX | WS_TABSTOP,
         DEDUP);
  defaults();
  SendMessageW(ctl(DEDUP), BM_SETCHECK,
               ini(L"Deduplicate", L"1") == L"0" ? BST_UNCHECKED : BST_CHECKED,
               0);
  SetWindowTextW(ctl(THREADS),
                 ini(L"Threads", text(ctl(THREADS)).c_str()).c_str());
  SetWindowTextW(ctl(PLY), ini(L"Ply", L"60").c_str());
  SetWindowTextW(ctl(MINIMUM), ini(L"Minimum", L"3").c_str());
  bar = create(PROGRESS_CLASSW, L"", 0, 201);
  SendMessageW(bar, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
  status = create(L"STATIC",
                  L"Select and check the source files, then click Build BIN.",
                  0, 202);
  enable(true);
  scan(programDir);
  layout();
  SetTimer(mainWindow, 1, 500, nullptr);
  ShowWindow(mainWindow, show);
  MSG msg;
  while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
    if (!IsDialogMessageW(mainWindow, &msg)) {
      TranslateMessage(&msg);
      DispatchMessageW(&msg);
    }
  }
  CoUninitialize();
  return 0;
}
