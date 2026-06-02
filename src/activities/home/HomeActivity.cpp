#include "HomeActivity.h"

#include <Bitmap.h>
#include <Epub.h>
#include <FsHelpers.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Memory.h>
#include <Utf8.h>
#include <Xtc.h>

#include <cstring>
#include <vector>

#include "CrossPointSettings.h"
#include "CrossPointState.h"
#include "MappedInputManager.h"
#include "OpdsServerStore.h"
#include "RecentBooksStore.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
constexpr char kMenuLabel[] = "Menu";

struct ResolvedHomePopupItem {
  ThemeHomeButtonAction action = ThemeHomeButtonAction::Default;
  std::string label;
  UIIcon icon = UIIcon::None;
};

bool isHomeButtonActionAvailable(const ThemeHomeButtonAction action, const bool hasOpdsServers,
                                 const bool hasRecentBooks) {
  switch (action) {
    case ThemeHomeButtonAction::Default:
    case ThemeHomeButtonAction::OpenHomePopup:
    case ThemeHomeButtonAction::FileBrowser:
    case ThemeHomeButtonAction::RecentBooks:
    case ThemeHomeButtonAction::FileTransfer:
    case ThemeHomeButtonAction::Settings:
      return true;
    case ThemeHomeButtonAction::OpdsBrowser:
      return hasOpdsServers;
    case ThemeHomeButtonAction::ContinueReading:
      return hasRecentBooks;
  }

  return true;
}

const char* defaultHomeButtonLabel(const ThemeHomeButtonAction action, const bool hasOpdsServers,
                                   const bool hasRecentBooks) {
  if (!isHomeButtonActionAvailable(action, hasOpdsServers, hasRecentBooks)) {
    return "";
  }

  switch (action) {
    case ThemeHomeButtonAction::OpenHomePopup:
      return kMenuLabel;
    case ThemeHomeButtonAction::FileBrowser:
      return tr(STR_BROWSE_FILES);
    case ThemeHomeButtonAction::RecentBooks:
      return tr(STR_MENU_RECENT_BOOKS);
    case ThemeHomeButtonAction::OpdsBrowser:
      return tr(STR_OPDS_BROWSER);
    case ThemeHomeButtonAction::FileTransfer:
      return tr(STR_FILE_TRANSFER);
    case ThemeHomeButtonAction::Settings:
      return tr(STR_SETTINGS_TITLE);
    case ThemeHomeButtonAction::ContinueReading:
      return tr(STR_CONTINUE_READING);
    case ThemeHomeButtonAction::Default:
      return "";
  }

  return "";
}

const ThemeHomeButtonBindingSpec& bindingForHardwareButton(const ThemeHomeHardwareButtonsSpec& spec,
                                                           const uint8_t hardwareButton) {
  switch (hardwareButton) {
    case HalGPIO::BTN_BACK:
      return spec.back;
    case HalGPIO::BTN_CONFIRM:
      return spec.confirm;
    case HalGPIO::BTN_LEFT:
      return spec.left;
    case HalGPIO::BTN_RIGHT:
    default:
      return spec.right;
  }
}

bool isHomePopupItemActionAvailable(const ThemeHomeButtonAction action, const bool hasOpdsServers,
                                    const bool hasRecentBooks) {
  switch (action) {
    case ThemeHomeButtonAction::FileBrowser:
    case ThemeHomeButtonAction::RecentBooks:
    case ThemeHomeButtonAction::FileTransfer:
    case ThemeHomeButtonAction::Settings:
      return true;
    case ThemeHomeButtonAction::OpdsBrowser:
      return hasOpdsServers;
    case ThemeHomeButtonAction::ContinueReading:
      return hasRecentBooks;
    case ThemeHomeButtonAction::Default:
      return false;
  }

  return false;
}

const char* defaultHomePopupItemLabel(const ThemeHomeButtonAction action) {
  switch (action) {
    case ThemeHomeButtonAction::FileBrowser:
      return tr(STR_BROWSE_FILES);
    case ThemeHomeButtonAction::RecentBooks:
      return tr(STR_MENU_RECENT_BOOKS);
    case ThemeHomeButtonAction::OpdsBrowser:
      return tr(STR_OPDS_BROWSER);
    case ThemeHomeButtonAction::FileTransfer:
      return tr(STR_FILE_TRANSFER);
    case ThemeHomeButtonAction::Settings:
      return tr(STR_SETTINGS_TITLE);
    case ThemeHomeButtonAction::ContinueReading:
      return tr(STR_CONTINUE_READING);
    default:
      return "";
  }
}

UIIcon defaultHomePopupItemIcon(const ThemeHomeButtonAction action) {
  switch (action) {
    case ThemeHomeButtonAction::FileBrowser:
      return UIIcon::Folder;
    case ThemeHomeButtonAction::RecentBooks:
      return UIIcon::Recent;
    case ThemeHomeButtonAction::OpdsBrowser:
      return UIIcon::Library;
    case ThemeHomeButtonAction::FileTransfer:
      return UIIcon::Transfer;
    case ThemeHomeButtonAction::Settings:
      return UIIcon::Settings;
    case ThemeHomeButtonAction::ContinueReading:
      return UIIcon::Book;
    default:
      return UIIcon::None;
  }
}

std::vector<ResolvedHomePopupItem> buildResolvedHomePopupItems(const ThemeHomePopupMenuSpec& spec,
                                                               const bool hasOpdsServers, const bool hasRecentBooks) {
  std::vector<ResolvedHomePopupItem> items;
  items.reserve(spec.items.size());
  for (const auto& itemSpec : spec.items) {
    if (!isHomePopupItemActionAvailable(itemSpec.action, hasOpdsServers, hasRecentBooks)) {
      continue;
    }

    ResolvedHomePopupItem item;
    item.action = itemSpec.action;
    item.label = itemSpec.label.empty() ? defaultHomePopupItemLabel(itemSpec.action) : itemSpec.label;
    item.icon = itemSpec.hasIcon ? itemSpec.icon : defaultHomePopupItemIcon(itemSpec.action);
    items.push_back(std::move(item));
  }
  return items;
}

bool anyFrontButtonEdge(MappedInputManager& mappedInput) {
  return mappedInput.wasPressed(MappedInputManager::Button::Back) ||
         mappedInput.wasPressed(MappedInputManager::Button::Confirm) ||
         mappedInput.wasPressed(MappedInputManager::Button::Left) ||
         mappedInput.wasPressed(MappedInputManager::Button::Right) ||
         mappedInput.wasReleased(MappedInputManager::Button::Back) ||
         mappedInput.wasReleased(MappedInputManager::Button::Confirm) ||
         mappedInput.wasReleased(MappedInputManager::Button::Left) ||
         mappedInput.wasReleased(MappedInputManager::Button::Right);
}

bool anyFrontButtonHeld(MappedInputManager& mappedInput) {
  return mappedInput.isPressed(MappedInputManager::Button::Back) ||
         mappedInput.isPressed(MappedInputManager::Button::Confirm) ||
         mappedInput.isPressed(MappedInputManager::Button::Left) ||
         mappedInput.isPressed(MappedInputManager::Button::Right);
}

const char* defaultHardwareButtonLabel(const uint8_t hardwareButton, const ThemeHomeButtonAction action,
                                       const bool hasOpdsServers, const bool hasRecentBooks) {
  (void)hardwareButton;
  return defaultHomeButtonLabel(action, hasOpdsServers, hasRecentBooks);
}
}  // namespace

int HomeActivity::getMenuItemCount() const {
  int count = 4;  // File Browser, Recents, File transfer, Settings
  if (!recentBooks.empty()) {
    count += recentBooks.size();
  }
  if (hasOpdsServers) {
    count++;
  }
  return count;
}

void HomeActivity::loadRecentBooks(int maxBooks) {
  recentBooks.clear();
  const auto& books = RECENT_BOOKS.getBooks();
  recentBooks.reserve(std::min(static_cast<int>(books.size()), maxBooks));

  for (const RecentBook& book : books) {
    // Limit to maximum number of recent books
    if (recentBooks.size() >= maxBooks) {
      break;
    }

    // Skip if file no longer exists
    if (RecentBooksStore::isMissing(book)) {
      continue;
    }

    recentBooks.push_back(book);
  }
}

void HomeActivity::loadRecentCovers(const std::vector<int>& coverHeights) {
  recentsLoading = true;
  bool showingLoading = false;
  Rect popupRect;

  int progress = 0;
  for (RecentBook& book : recentBooks) {
    if (!book.coverBmpPath.empty()) {
      bool hasMissingThumb = false;
      for (const int coverHeight : coverHeights) {
        std::string coverPath = UITheme::getCoverThumbPath(book.coverBmpPath, coverHeight);
        if (!Storage.exists(coverPath.c_str())) {
          hasMissingThumb = true;
          break;
        }
      }

      if (hasMissingThumb) {
        // If epub, try to load the metadata for title/author and cover
        if (FsHelpers::hasEpubExtension(book.path)) {
          Epub epub(book.path, "/.crosspoint");
          // Skip loading css since we only need metadata here
          epub.load(false, true);

          // Try to generate thumbnail image for Continue Reading card
          if (!showingLoading) {
            showingLoading = true;
            popupRect = GUI.drawPopup(renderer, tr(STR_LOADING_POPUP));
          }
          GUI.fillPopupProgress(renderer, popupRect, 10 + progress * (90 / recentBooks.size()));
          bool success = true;
          for (const int coverHeight : coverHeights) {
            std::string coverPath = UITheme::getCoverThumbPath(book.coverBmpPath, coverHeight);
            if (!Storage.exists(coverPath.c_str())) {
              success = epub.generateThumbBmp(coverHeight) && success;
            }
          }
          if (!success) {
            RECENT_BOOKS.updateBook(book.path, book.title, book.author, "");
            book.coverBmpPath = "";
          }
          coverRendered = false;
          requestUpdate();
        } else if (FsHelpers::hasXtcExtension(book.path)) {
          // Handle XTC file
          Xtc xtc(book.path, "/.crosspoint");
          if (xtc.load()) {
            // Try to generate thumbnail image for Continue Reading card
            if (!showingLoading) {
              showingLoading = true;
              popupRect = GUI.drawPopup(renderer, tr(STR_LOADING_POPUP));
            }
            GUI.fillPopupProgress(renderer, popupRect, 10 + progress * (90 / recentBooks.size()));
            bool success = true;
            for (const int coverHeight : coverHeights) {
              std::string coverPath = UITheme::getCoverThumbPath(book.coverBmpPath, coverHeight);
              if (!Storage.exists(coverPath.c_str())) {
                success = xtc.generateThumbBmp(coverHeight) && success;
              }
            }
            if (!success) {
              RECENT_BOOKS.updateBook(book.path, book.title, book.author, "");
              book.coverBmpPath = "";
            }
            coverRendered = false;
            requestUpdate();
          }
        }
      }
    }
    progress++;
  }

  recentsLoaded = true;
  recentsLoading = false;
}

void HomeActivity::onEnter() {
  Activity::onEnter();
  suppressFrontButtonReleaseActions = true;

  hasOpdsServers = OPDS_STORE.hasServers();

  const auto& metrics = UITheme::getInstance().getMetrics();
  loadRecentBooks(metrics.homeRecentBooksCount);
  LOG_DBG("HOME", "Loaded %d/%d recent book(s) for home theme", static_cast<int>(recentBooks.size()),
          metrics.homeRecentBooksCount);

  const auto base = static_cast<int>(recentBooks.size());
  selectorIndex = initialMenuItem == HomeMenuItem::NONE ? 0 : base + menuItemToIndex(initialMenuItem, hasOpdsServers);
  coverSelectorIndex = recentBooks.empty() ? 0 : std::min(selectorIndex, static_cast<int>(recentBooks.size()) - 1);
  homePopupOpen = false;
  homePopupIndex = 0;

  // Trigger first update
  requestUpdate();
}

void HomeActivity::onExit() {
  Activity::onExit();

  // Free the stored cover buffer if any
  freeCoverBuffer();
}

bool HomeActivity::storeCoverBuffer() {
  // render() must have already set the cover rect; without it we'd be back to
  // cloning the whole framebuffer.
  if (coverRectW <= 0 || coverRectH <= 0) return false;
  freeCoverBuffer();
  const size_t needed = renderer.getRegionByteSize(coverRectX, coverRectY, coverRectW, coverRectH);
  if (needed == 0) return false;
  coverBuffer = makeUniqueNoThrow<uint8_t[]>(needed);
  if (!coverBuffer) {
    LOG_ERR("HOME", "OOM: cover buffer (%u bytes)", (unsigned)needed);
    return false;
  }
  coverBufferSize = needed;
  if (!renderer.copyRegionToBuffer(coverRectX, coverRectY, coverRectW, coverRectH, coverBuffer.get(),
                                   coverBufferSize)) {
    coverBuffer.reset();
    coverBufferSize = 0;
    return false;
  }
  coverBufferSelectorIndex = coverSelectorIndex;
  coverBufferStripSelected = selectorIndex < static_cast<int>(recentBooks.size());
  return true;
}

bool HomeActivity::restoreCoverBuffer() {
  if (!coverBuffer || coverRectW <= 0 || coverRectH <= 0) return false;
  return renderer.copyBufferToRegion(coverRectX, coverRectY, coverRectW, coverRectH, coverBuffer.get(),
                                     coverBufferSize);
}

void HomeActivity::freeCoverBuffer() {
  coverBuffer.reset();
  coverBufferSize = 0;
  coverBufferStored = false;
  coverBufferSelectorIndex = -1;
  coverBufferStripSelected = false;
}

void HomeActivity::selectNextHomeItem(const int menuCount) {
  selectorIndex = ButtonNavigator::nextIndex(selectorIndex, menuCount);
  if (selectorIndex < static_cast<int>(recentBooks.size())) {
    coverSelectorIndex = selectorIndex;
  }
  requestUpdate();
}

void HomeActivity::selectPreviousHomeItem(const int menuCount) {
  selectorIndex = ButtonNavigator::previousIndex(selectorIndex, menuCount);
  if (selectorIndex < static_cast<int>(recentBooks.size())) {
    coverSelectorIndex = selectorIndex;
  }
  requestUpdate();
}

void HomeActivity::activateCurrentSelection() {
  if (selectorIndex < static_cast<int>(recentBooks.size())) {
    onSelectBook(recentBooks[selectorIndex].path);
    return;
  }

  const int menuIndex = selectorIndex - static_cast<int>(recentBooks.size());
  switch (indexToMenuItem(menuIndex, hasOpdsServers)) {
    case HomeMenuItem::FILE_BROWSER:
      onFileBrowserOpen();
      break;
    case HomeMenuItem::RECENTS:
      onRecentsOpen();
      break;
    case HomeMenuItem::OPDS_BROWSER:
      onOpdsBrowserOpen();
      break;
    case HomeMenuItem::FILE_TRANSFER:
      onFileTransferOpen();
      break;
    case HomeMenuItem::SETTINGS_MENU:
      onSettingsOpen();
      break;
    default:
      break;
  }
}

void HomeActivity::closeHomePopup() {
  homePopupOpen = false;
  homePopupIndex = 0;
}

void HomeActivity::loop() {
  if (suppressFrontButtonReleaseActions) {
    // Swallow the press/release that brought us back to Home so release-driven
    // theme actions do not fire across activity transitions.
    const bool frontButtonHeld = anyFrontButtonHeld(mappedInput);
    const bool frontButtonEdge = anyFrontButtonEdge(mappedInput);
    if (frontButtonHeld || frontButtonEdge) {
      if (!frontButtonHeld) {
        suppressFrontButtonReleaseActions = false;
      }
      return;
    }
    suppressFrontButtonReleaseActions = false;
  }

  const int menuCount = getMenuItemCount();
  const ThemeHomeHardwareButtonsSpec* homeHardwareButtons = UITheme::getInstance().getHomeHardwareButtons();
  const ThemeHomePopupMenuSpec* homePopupMenu = UITheme::getInstance().getHomePopupMenu();
  const auto runHomeButtonAction = [&](const ThemeHomeButtonAction action) -> bool {
    switch (action) {
      case ThemeHomeButtonAction::Default:
        return false;
      case ThemeHomeButtonAction::OpenHomePopup:
        if (homePopupMenu != nullptr && homePopupMenu->enabled) {
          auto popupItems = buildResolvedHomePopupItems(*homePopupMenu, hasOpdsServers, !recentBooks.empty());
          if (!popupItems.empty()) {
            homePopupOpen = true;
            homePopupIndex = 0;
            requestUpdate();
          }
        }
        return true;
      case ThemeHomeButtonAction::FileBrowser:
        onFileBrowserOpen();
        return true;
      case ThemeHomeButtonAction::RecentBooks:
        onRecentsOpen();
        return true;
      case ThemeHomeButtonAction::OpdsBrowser:
        if (hasOpdsServers) {
          onOpdsBrowserOpen();
        }
        return true;
      case ThemeHomeButtonAction::FileTransfer:
        onFileTransferOpen();
        return true;
      case ThemeHomeButtonAction::Settings:
        onSettingsOpen();
        return true;
      case ThemeHomeButtonAction::ContinueReading:
        if (!recentBooks.empty()) {
          onSelectBook(recentBooks[std::min(coverSelectorIndex, static_cast<int>(recentBooks.size()) - 1)].path);
        }
        return true;
    }

    return false;
  };
  if (homePopupOpen && homePopupMenu != nullptr) {
    auto popupItems = buildResolvedHomePopupItems(*homePopupMenu, hasOpdsServers, !recentBooks.empty());
    const int popupCount = static_cast<int>(popupItems.size());
    if (popupCount <= 0) {
      closeHomePopup();
      requestUpdate();
      return;
    }
    if (homePopupIndex >= popupCount) {
      homePopupIndex = popupCount - 1;
    }

    if (mappedInput.wasPressed(MappedInputManager::Button::Up) || mappedInput.wasRawButtonPressed(HalGPIO::BTN_LEFT)) {
      homePopupIndex = ButtonNavigator::previousIndex(homePopupIndex, popupCount);
      requestUpdate();
      return;
    }
    if (mappedInput.wasPressed(MappedInputManager::Button::Down) ||
        mappedInput.wasRawButtonPressed(HalGPIO::BTN_RIGHT)) {
      homePopupIndex = ButtonNavigator::nextIndex(homePopupIndex, popupCount);
      requestUpdate();
      return;
    }
    if (mappedInput.wasRawButtonReleased(HalGPIO::BTN_BACK)) {
      closeHomePopup();
      requestUpdate();
      return;
    }
    if (mappedInput.wasRawButtonReleased(HalGPIO::BTN_CONFIRM)) {
      const auto action = popupItems[homePopupIndex].action;
      closeHomePopup();
      runHomeButtonAction(action);
      return;
    }
    return;
  }

  if (homeHardwareButtons != nullptr) {
    if (mappedInput.wasPressed(MappedInputManager::Button::Up)) {
      selectPreviousHomeItem(menuCount);
      return;
    }
    if (mappedInput.wasPressed(MappedInputManager::Button::Down)) {
      selectNextHomeItem(menuCount);
      return;
    }

    const auto handleHomeHardwareButton = [&](const uint8_t hardwareButton) -> bool {
      const auto& binding = bindingForHardwareButton(*homeHardwareButtons, hardwareButton);
      if (binding.action == ThemeHomeButtonAction::Default ||
          !isHomeButtonActionAvailable(binding.action, hasOpdsServers, !recentBooks.empty())) {
        return false;
      }
      if (mappedInput.wasRawButtonReleased(hardwareButton)) {
        return runHomeButtonAction(binding.action);
      }
      return false;
    };

    if (handleHomeHardwareButton(HalGPIO::BTN_BACK) || handleHomeHardwareButton(HalGPIO::BTN_CONFIRM) ||
        handleHomeHardwareButton(HalGPIO::BTN_LEFT) || handleHomeHardwareButton(HalGPIO::BTN_RIGHT)) {
      return;
    }
    return;
  }

  buttonNavigator.onNext([this, menuCount] { selectNextHomeItem(menuCount); });

  buttonNavigator.onPrevious([this, menuCount] { selectPreviousHomeItem(menuCount); });

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    activateCurrentSelection();
  }
}

void HomeActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  const ThemeButtonMenuSpec* homeButtonMenu = UITheme::getInstance().getHomeButtonMenu();
  const ThemeHomeHardwareButtonsSpec* homeHardwareButtons = UITheme::getInstance().getHomeHardwareButtons();
  const ThemeHomePopupMenuSpec* homePopupMenu = UITheme::getInstance().getHomePopupMenu();
  constexpr int coverCacheBleed = 12;
  const bool hasCoverArea = metrics.homeCoverTileHeight > 0 && metrics.homeCoverHeight > 0;

  renderer.clearScreen();

  if (homePopupOpen && homePopupMenu != nullptr) {
    auto popupItems = buildResolvedHomePopupItems(*homePopupMenu, hasOpdsServers, !recentBooks.empty());
    if (popupItems.empty()) {
      closeHomePopup();
    } else {
      if (homePopupIndex >= static_cast<int>(popupItems.size())) {
        homePopupIndex = static_cast<int>(popupItems.size()) - 1;
      }
      Rect popupMenuRect{0, metrics.homeTopPadding, pageWidth, pageHeight - metrics.homeTopPadding};
      if (homeButtonMenu != nullptr && homeButtonMenu->centerVertically) {
        popupMenuRect = Rect{0, 0, pageWidth, std::max(0, pageHeight - metrics.buttonHintsHeight)};
      }
      GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.homeTopPadding}, nullptr);
      GUI.drawButtonMenu(
          renderer, popupMenuRect, static_cast<int>(popupItems.size()), homePopupIndex,
          [&popupItems](int index) { return popupItems[index].label; },
          [&popupItems](int index) { return popupItems[index].icon; });
      GUI.drawButtonHints(renderer, tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
      renderer.displayBuffer();
      return;
    }
  }

  // Record the tile rect so storeCoverBuffer (called from the theme) knows
  // which sub-region of the framebuffer to snapshot. Include a small bleed
  // because cover-strip themes can draw selection outlines just outside the
  // nominal cover tile.
  coverRectX = 0;
  coverRectY = hasCoverArea ? std::max(0, metrics.homeTopPadding - coverCacheBleed) : 0;
  coverRectW = pageWidth;
  coverRectH = hasCoverArea
                   ? std::min(pageHeight - coverRectY,
                              metrics.homeCoverTileHeight + (metrics.homeTopPadding - coverRectY) + coverCacheBleed)
                   : 0;

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.homeTopPadding},
                 metrics.homeContinueReadingInMenu && metrics.homeShowContinueReadingHeader && !recentBooks.empty()
                     ? recentBooks[std::min(coverSelectorIndex, static_cast<int>(recentBooks.size()) - 1)].title.c_str()
                     : nullptr);

  const bool selectorSensitiveCoverCache = GUI.homeCoverCacheDependsOnSelector();
  const bool coverStripSelected = selectorIndex < static_cast<int>(recentBooks.size());
  bool bufferRestored = hasCoverArea && coverBufferStored &&
                        (!selectorSensitiveCoverCache || (coverBufferSelectorIndex == coverSelectorIndex &&
                                                          coverBufferStripSelected == coverStripSelected)) &&
                        restoreCoverBuffer();

  if (hasCoverArea) {
    GUI.drawRecentBookCover(renderer, Rect{0, metrics.homeTopPadding, pageWidth, metrics.homeCoverTileHeight},
                            recentBooks, coverSelectorIndex, coverRendered, coverBufferStored, bufferRestored,
                            std::bind(&HomeActivity::storeCoverBuffer, this), coverStripSelected);
  } else {
    coverRendered = false;
    coverBufferStored = false;
    bufferRestored = false;
  }

  // Build menu items dynamically
  std::vector<const char*> menuItems = {tr(STR_BROWSE_FILES), tr(STR_MENU_RECENT_BOOKS), tr(STR_FILE_TRANSFER),
                                        tr(STR_SETTINGS_TITLE)};
  std::vector<UIIcon> menuIcons = {Folder, Recent, Transfer, Settings};

  if (hasOpdsServers) {
    menuItems.insert(menuItems.begin() + 2, tr(STR_OPDS_BROWSER));
    menuIcons.insert(menuIcons.begin() + 2, Library);
  }

  if (metrics.homeContinueReadingInMenu && !recentBooks.empty()) {
    // Insert Continue Reading at the top if enabled in theme
    menuItems.insert(menuItems.begin(), tr(STR_CONTINUE_READING));
    menuIcons.insert(menuIcons.begin(), Book);
  }

  if (homeButtonMenu == nullptr || homeButtonMenu->showOnHome) {
    const int menuTop = metrics.homeTopPadding + metrics.homeCoverTileHeight + metrics.homeMenuTopOffset;
    const int menuHeight = std::max(0, pageHeight - menuTop - metrics.buttonHintsHeight - metrics.verticalSpacing);
    GUI.drawButtonMenu(
        renderer, Rect{0, menuTop, pageWidth, menuHeight}, static_cast<int>(menuItems.size()),
        metrics.homeContinueReadingInMenu ? selectorIndex : selectorIndex - recentBooks.size(),
        [&menuItems](int index) { return std::string(menuItems[index]); },
        [&menuIcons](int index) { return menuIcons[index]; });
  }

  if (homeHardwareButtons != nullptr) {
    const auto labelForHardwareBinding = [&](const uint8_t hardwareButton,
                                             const ThemeHomeButtonBindingSpec& binding) -> const char* {
      if (binding.action == ThemeHomeButtonAction::Default ||
          !isHomeButtonActionAvailable(binding.action, hasOpdsServers, !recentBooks.empty())) {
        return "";
      }
      if (!binding.label.empty()) {
        return binding.label.c_str();
      }
      return defaultHardwareButtonLabel(hardwareButton, binding.action, hasOpdsServers, !recentBooks.empty());
    };
    GUI.drawButtonHints(renderer, labelForHardwareBinding(HalGPIO::BTN_BACK, homeHardwareButtons->back),
                        labelForHardwareBinding(HalGPIO::BTN_CONFIRM, homeHardwareButtons->confirm),
                        labelForHardwareBinding(HalGPIO::BTN_LEFT, homeHardwareButtons->left),
                        labelForHardwareBinding(HalGPIO::BTN_RIGHT, homeHardwareButtons->right));
  } else {
    const auto labels = mappedInput.mapLabels("", tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  }

  renderer.displayBuffer();

  if (!firstRenderDone) {
    firstRenderDone = true;
    requestUpdate();
  } else if (metrics.homeCoverHeight > 0 && !recentsLoaded && !recentsLoading) {
    recentsLoading = true;
    loadRecentCovers(UITheme::getInstance().getHomeCoverThumbHeights());
  }
}

void HomeActivity::onSelectBook(const std::string& path) { activityManager.goToReader(path); }

void HomeActivity::onFileBrowserOpen() { activityManager.goToFileBrowser(); }

void HomeActivity::onRecentsOpen() { activityManager.goToRecentBooks(); }

void HomeActivity::onSettingsOpen() { activityManager.goToSettings(); }

void HomeActivity::onFileTransferOpen() { activityManager.goToFileTransfer(); }

void HomeActivity::onOpdsBrowserOpen() { activityManager.goToBrowser(); }
