#pragma once

#include <string>

#include "BookReadingStats.h"
#include "GlobalReadingStats.h"

class GfxRenderer;
class MappedInputManager;

void renderPerBookStatsPage(GfxRenderer& renderer, const MappedInputManager* mappedInput, const std::string& bookTitle,
                            const BookReadingStats& stats, float progressPercent, bool hasEstimatedTimeLeft,
                            uint32_t estimatedTimeLeftSeconds, bool showButtonHints, bool showEditButton,
                            bool showMoreButton);

void renderGlobalStatsPage(GfxRenderer& renderer, const MappedInputManager* mappedInput, const char* screenTitle,
                           const GlobalReadingStats& stats, bool showButtonHints, bool showMoreButton);

void renderEditBookDatesPage(GfxRenderer& renderer, const MappedInputManager* mappedInput, const std::string& bookTitle,
                             const BookReadingStats& stats, int selectedField, bool showButtonHints);
