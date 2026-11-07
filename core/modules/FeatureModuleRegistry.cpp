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
