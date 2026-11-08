#include "interfaceDefs.h"

#include "FeatureModuleRegistry.h"

#include <cstdio>
#include <cstring>

constexpr uint8_t InterfaceGlobals::fxBuiltinIDs[FX_BUILTIN_COUNT];
constexpr char InterfaceGlobals::rndFxName[FX_NAME_SIZE + 4];
constexpr char InterfaceGlobals::fxBuiltinNames[FX_BUILTIN_COUNT][FX_NAME_SIZE + 4];
constexpr char InterfaceGlobals::fxBuiltinShortNames[FX_BUILTIN_COUNT][FX_NAME_SIZE + 4];

void InterfaceGlobals::rebuildPatternFxCatalog()
{
    const uint8_t hidden = FX_COUNT_HIDDEN_FXes;
    const uint8_t visibleBuiltin =
      FX_BUILTIN_COUNT > hidden ? static_cast<uint8_t>(FX_BUILTIN_COUNT - hidden) : FX_BUILTIN_COUNT;

    fxCount = 0;
    memset(fxIDs, 0, sizeof(fxIDs));
    memset(fxNames, 0, sizeof(fxNames));
    memset(fxShortNames, 0, sizeof(fxShortNames));
    memset(fxDescriptionPtrs, 0, sizeof(fxDescriptionPtrs));
    memset(ptrAllFxNames, 0, sizeof(ptrAllFxNames));
    memset(ptrAllFxNamesShort, 0, sizeof(ptrAllFxNamesShort));

    auto appendEntry = [&](uint8_t typeId,
                           const char* longName,
                           const char* shortName,
                           const char* description) {
        if (fxCount >= FX_COUNT_MAX || longName == nullptr || shortName == nullptr)
        {
            return;
        }
        fxIDs[fxCount] = typeId;
        strncpy(fxNames[fxCount], longName, FX_NAME_SIZE + 3);
        fxNames[fxCount][FX_NAME_SIZE + 3] = '\0';
        strncpy(fxShortNames[fxCount], shortName, FX_NAME_SIZE + 3);
        fxShortNames[fxCount][FX_NAME_SIZE + 3] = '\0';
        fxDescriptionPtrs[fxCount] = description;
        fxCount++;
    };

    for (uint8_t i = 0; i < visibleBuiltin; i++)
    {
        appendEntry(fxBuiltinIDs[i],
                    fxBuiltinNames[i],
                    fxBuiltinShortNames[i],
                    fxBuiltinDescriptions[i]);
    }

    for (uint8_t m = 0; m < featureModules.patternStepFxCount(); m++)
    {
        const PatternStepFxInfo* info = featureModules.patternStepFxAt(m);
        if (info == nullptr)
        {
            continue;
        }

        bool letterTaken = false;
        for (uint8_t i = 0; i < fxCount; i++)
        {
            if (fxNames[i][0] == info->letter)
            {
                letterTaken = true;
                break;
            }
        }
        if (letterTaken || fxCount >= FX_COUNT_MAX - hidden)
        {
            continue;
        }

        char longName[FX_NAME_SIZE + 4];
        snprintf(longName,
                 sizeof(longName),
                 "%c - %.*s",
                 info->letter,
                 kPatternStepFxNameMax,
                 info->name != nullptr ? info->name : "");

        const char* shortName =
          info->shortName != nullptr && info->shortName[0] != '\0' ? info->shortName : info->name;
        const char* description =
          info->description != nullptr ? info->description : "";

        appendEntry(info->typeId, longName, shortName, description);
    }

    for (uint8_t i = visibleBuiltin; i < FX_BUILTIN_COUNT; i++)
    {
        appendEntry(fxBuiltinIDs[i],
                    fxBuiltinNames[i],
                    fxBuiltinShortNames[i],
                    fxBuiltinDescriptions[i]);
    }

    ptrAllFxNames[0] = rndFxName;
    ptrAllFxNamesShort[0] = rndFxName;
    for (uint8_t i = 0; i < fxCount; i++)
    {
        ptrAllFxNames[i + 1] = fxNames[i];
        ptrAllFxNamesShort[i + 1] = fxShortNames[i];
    }
    ptrFxNames = &ptrAllFxNames[1];
}
