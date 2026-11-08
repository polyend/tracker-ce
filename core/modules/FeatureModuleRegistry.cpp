#include "FeatureModuleRegistry.h"

FeatureModuleRegistry featureModules;

void FeatureModuleRegistry::add(FeatureModule* module)
{
    if (module == nullptr || moduleCount >= kMaxModules)
    {
        return;
    }

    modules[moduleCount++] = module;
}

void FeatureModuleRegistry::clear()
{
    moduleCount = 0;
    patternFxCount = 0;
}

FeatureModule* FeatureModuleRegistry::get(uint8_t index) const
{
    if (index >= moduleCount)
    {
        return nullptr;
    }
    return modules[index];
}

void FeatureModuleRegistry::initAll(FileSystem* fs)
{
    for (uint8_t i = 0; i < moduleCount; i++)
    {
        modules[i]->init(fs);
    }

    collectPatternStepFx();
}

void FeatureModuleRegistry::onWorkspaceClear()
{
    for (uint8_t i = 0; i < moduleCount; i++)
    {
        modules[i]->onWorkspaceClear();
    }
}

void FeatureModuleRegistry::onWorkspaceLoad()
{
    for (uint8_t i = 0; i < moduleCount; i++)
    {
        modules[i]->onWorkspaceLoad();
    }
}

void FeatureModuleRegistry::onProjectOpen(const char* projectPath, const char* projectName)
{
    for (uint8_t i = 0; i < moduleCount; i++)
    {
        modules[i]->onProjectOpen(projectPath, projectName);
    }
}

void FeatureModuleRegistry::onProjectSave(const char* projectPath, const char* projectName)
{
    for (uint8_t i = 0; i < moduleCount; i++)
    {
        modules[i]->onProjectSave(projectPath, projectName);
    }
}

void FeatureModuleRegistry::updateAll()
{
    for (uint8_t i = 0; i < moduleCount; i++)
    {
        modules[i]->update();
    }

    pollSongListColumnSlotStarts();
}

uint8_t FeatureModuleRegistry::performancePageCount() const
{
    uint8_t pages = 0;
    for (uint8_t i = 0; i < moduleCount; i++)
    {
        if (modules[i]->performancePage() != nullptr)
        {
            pages++;
        }
    }
    return pages;
}

PerformancePage* FeatureModuleRegistry::getPerformancePage(uint8_t index) const
{
    uint8_t pages = 0;
    for (uint8_t i = 0; i < moduleCount; i++)
    {
        PerformancePage* page = modules[i]->performancePage();
        if (page == nullptr)
        {
            continue;
        }
        if (pages == index)
        {
            return page;
        }
        pages++;
    }
    return nullptr;
}

SongListColumn* FeatureModuleRegistry::getSongListColumn() const
{
    for (uint8_t i = 0; i < moduleCount; i++)
    {
        ScreenExtension* extension = modules[i]->screenExtension();
        if (extension == nullptr || extension->screen() != ScreenId::Song)
        {
            continue;
        }

        SongListColumn* column = extension->asSongListColumn();
        if (column != nullptr)
        {
            return column;
        }
    }
    return nullptr;
}

bool FeatureModuleRegistry::isPatternStepFxLetterTaken(char letter) const
{
    for (uint8_t i = 0; i < patternFxCount; i++)
    {
        if (patternFx[i] != nullptr && patternFx[i]->letter == letter)
        {
            return true;
        }
    }
    return false;
}

void FeatureModuleRegistry::collectPatternStepFx()
{
    patternFxCount = 0;

    for (uint8_t m = 0; m < moduleCount; m++)
    {
        const uint8_t fxCount = modules[m]->patternStepFxCount();
        for (uint8_t f = 0; f < fxCount; f++)
        {
            const PatternStepFxInfo* info = modules[m]->patternStepFx(f);
            if (info == nullptr)
            {
                continue;
            }
            if (info->typeId < kPatternStepFxModuleIdBase ||
                info->typeId > kPatternStepFxModuleIdLast)
            {
                continue;
            }
            if (info->letter == '\0' || info->name == nullptr || info->name[0] == '\0')
            {
                continue;
            }
            if (findPatternStepFx(info->typeId) != nullptr)
            {
                continue;
            }
            if (isPatternStepFxLetterTaken(info->letter))
            {
                continue;
            }
            if (patternFxCount >= kMaxModulePatternStepFx)
            {
                return;
            }

            patternFx[patternFxCount++] = info;
        }
    }
}

const PatternStepFxInfo* FeatureModuleRegistry::patternStepFxAt(uint8_t index) const
{
    if (index >= patternFxCount)
    {
        return nullptr;
    }
    return patternFx[index];
}

const PatternStepFxInfo* FeatureModuleRegistry::findPatternStepFx(uint8_t typeId) const
{
    for (uint8_t i = 0; i < patternFxCount; i++)
    {
        if (patternFx[i] != nullptr && patternFx[i]->typeId == typeId)
        {
            return patternFx[i];
        }
    }
    return nullptr;
}

void FeatureModuleRegistry::dispatchPatternStepFx(uint8_t typeId, uint8_t value)
{
    for (uint8_t i = 0; i < moduleCount; i++)
    {
        if (modules[i]->onPatternStepFx(typeId, value))
        {
            return;
        }
    }
}
