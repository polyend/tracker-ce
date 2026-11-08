#include "SongFxScenes.h"

#include "ModuleSidecar.h"
#include "Platform.h"

#include <cr_section_macros.h>
#include <cstdio>
#include <cstring>

__NOINIT(EXTERNAL_RAM) static strSongFxScenesFile externalRamSongFxScenesBank;

SongFxScenes songFxScenes;

static const PatternStepFxInfo kSongFxScenePatternStepFx = {
    kSongFxScenePatternStepFxId,
    'N',
    "FX Scene",
    "FX Scene",
    "Apply Song FX scene 1-8.",
    1,
    kSongFxScenesCount,
    1,
};

const PatternStepFxInfo* SongFxScenes::patternStepFx(uint8_t index) const
{
    return index == 0 ? &kSongFxScenePatternStepFx : nullptr;
}

bool SongFxScenes::onPatternStepFx(uint8_t typeId, uint8_t value)
{
    if (typeId != kSongFxScenePatternStepFxId)
    {
        return false;
    }

    if (value < 1 || value > kSongFxScenesCount)
    {
        return true;
    }

    applyScene(static_cast<uint8_t>(value - 1));
    return true;
}

void SongFxScenes::init(FileSystem* fileSystem)
{
    this->fileSystem = fileSystem;
    clearBank();
}

void SongFxScenes::onWorkspaceClear()
{
    clearBank();
}

void SongFxScenes::onWorkspaceLoad()
{
    char path[ModuleSidecar::kMaxPathLength];
    ModuleSidecar::workspacePath(path, sizeof(path), id());

    if (!readFile(path))
    {
        clearBank();
        if (fileSystem != nullptr && fileSystem->exists(path))
        {
            fileSystem->remove(path);
        }
    }
}

void SongFxScenes::onProjectOpen(const char* projectPath, const char* projectName)
{
    char srcPath[ModuleSidecar::kMaxPathLength];
    ModuleSidecar::projectPath(srcPath, sizeof(srcPath), projectPath, projectName, id());

    char workspacePathBuf[ModuleSidecar::kMaxPathLength];
    ModuleSidecar::workspacePath(workspacePathBuf, sizeof(workspacePathBuf), id());

    if (!readFile(srcPath))
    {
        clearBank();
        if (fileSystem != nullptr && fileSystem->exists(workspacePathBuf))
        {
            fileSystem->remove(workspacePathBuf);
        }
        return;
    }

    writeFile(workspacePathBuf);
}

void SongFxScenes::onProjectSave(const char* projectPath, const char* projectName)
{
    if (fileSystem == nullptr)
    {
        return;
    }

    char workspacePathBuf[ModuleSidecar::kMaxPathLength];
    ModuleSidecar::workspacePath(workspacePathBuf, sizeof(workspacePathBuf), id());

    if (!fileSystem->exists(workspacePathBuf))
    {
        return;
    }

    if (!readFile(workspacePathBuf))
    {
        return;
    }

    char destPath[ModuleSidecar::kMaxPathLength];
    ModuleSidecar::projectPath(destPath, sizeof(destPath), projectPath, projectName, id());
    writeFile(destPath);
}

void SongFxScenes::clearBank()
{
    memset(&externalRamSongFxScenesBank, 0, sizeof(externalRamSongFxScenesBank));
    externalRamSongFxScenesBank.magic[0] = 'S';
    externalRamSongFxScenesBank.magic[1] = 'F';
    externalRamSongFxScenesBank.magic[2] = 'X';
    externalRamSongFxScenesBank.magic[3] = 'S';
    externalRamSongFxScenesBank.version = kSongFxScenesFileVersion;
    externalRamSongFxScenesBank.count = kSongFxScenesCount;
    editingScene = 0;
    focusedEntry = 0;
}

bool SongFxScenes::validateHeader() const
{
    return externalRamSongFxScenesBank.magic[0] == 'S' &&
           externalRamSongFxScenesBank.magic[1] == 'F' &&
           externalRamSongFxScenesBank.magic[2] == 'X' &&
           externalRamSongFxScenesBank.magic[3] == 'S' &&
           externalRamSongFxScenesBank.version == kSongFxScenesFileVersion &&
           externalRamSongFxScenesBank.count == kSongFxScenesCount;
}

void SongFxScenes::clampSongSlotScenes()
{
    for (uint8_t i = 0; i < SongListColumn::kSongSlotCount; i++)
    {
        if (externalRamSongFxScenesBank.songSlotScene[i] > kSongFxScenesCount)
        {
            externalRamSongFxScenesBank.songSlotScene[i] = kSongFxScenesNone;
        }
    }
    externalRamSongFxScenesBank.songSlotScene[SongListColumn::kSongSlotCount] = 0;
}

bool SongFxScenes::readFile(const char* path)
{
    if (!ModuleSidecar::read(
          fileSystem, path, &externalRamSongFxScenesBank, sizeof(externalRamSongFxScenesBank)))
    {
        return false;
    }

    if (!validateHeader())
    {
        clearBank();
        return false;
    }

    clampSongSlotScenes();
    return true;
}

void SongFxScenes::writeFile(const char* path)
{
    externalRamSongFxScenesBank.magic[0] = 'S';
    externalRamSongFxScenesBank.magic[1] = 'F';
    externalRamSongFxScenesBank.magic[2] = 'X';
    externalRamSongFxScenesBank.magic[3] = 'S';
    externalRamSongFxScenesBank.version = kSongFxScenesFileVersion;
    externalRamSongFxScenesBank.count = kSongFxScenesCount;
    clampSongSlotScenes();

    ModuleSidecar::write(
      fileSystem, path, &externalRamSongFxScenesBank, sizeof(externalRamSongFxScenesBank));
}

void SongFxScenes::saveToWorkspace()
{
    char path[ModuleSidecar::kMaxPathLength];
    ModuleSidecar::workspacePath(path, sizeof(path), id());
    writeFile(path);
}

uint8_t SongFxScenes::sceneForSlot(uint8_t slot) const
{
    if (slot >= SongListColumn::kSongSlotCount)
    {
        return kSongFxScenesNone;
    }
    return externalRamSongFxScenesBank.songSlotScene[slot];
}

void SongFxScenes::assignSlot(uint8_t slot, uint8_t scene)
{
    if (slot >= SongListColumn::kSongSlotCount)
    {
        return;
    }

    if (scene > kSongFxScenesCount)
    {
        scene = kSongFxScenesNone;
    }

    if (externalRamSongFxScenesBank.songSlotScene[slot] == scene)
    {
        return;
    }

    externalRamSongFxScenesBank.songSlotScene[slot] = scene;
    saveToWorkspace();
}

void SongFxScenes::formatCell(uint8_t slot, char* out, uint8_t outSize) const
{
    if (out == nullptr || outSize == 0)
    {
        return;
    }

    out[0] = '\0';
    if (slot >= SongListColumn::kSongSlotCount)
    {
        return;
    }

    const uint8_t value = sceneForSlot(slot);
    if (value == kSongFxScenesNone)
    {
        snprintf(out, outSize, "-");
        return;
    }

    snprintf(out, outSize, "%u", static_cast<unsigned>(value));
}

void SongFxScenes::onEncoder(SongScreenHost& host, uint8_t slot, int8_t delta)
{
    (void)host;
    if (slot >= SongListColumn::kSongSlotCount || delta == 0)
    {
        return;
    }

    int16_t value = sceneForSlot(slot);
    value += delta;

    const int16_t maxValue = kSongFxScenesCount;
    while (value < 0)
    {
        value += maxValue + 1;
    }
    while (value > maxValue)
    {
        value -= maxValue + 1;
    }

    assignSlot(slot, static_cast<uint8_t>(value));
}

void SongFxScenes::onClear(SongScreenHost& host, uint8_t slot)
{
    (void)host;
    assignSlot(slot, kSongFxScenesNone);
}

void SongFxScenes::onInserted(uint8_t index, bool duplicate)
{
    if (index >= SongListColumn::kSongSlotCount - 1)
    {
        return;
    }

    for (int i = SongListColumn::kSongSlotCount - 1; i > index; i--)
    {
        externalRamSongFxScenesBank.songSlotScene[i] =
          externalRamSongFxScenesBank.songSlotScene[i - 1];
    }

    const uint8_t insertAt = static_cast<uint8_t>(index + 1);
    if (duplicate)
    {
        externalRamSongFxScenesBank.songSlotScene[insertAt] =
          externalRamSongFxScenesBank.songSlotScene[index];
    }
    else
    {
        externalRamSongFxScenesBank.songSlotScene[insertAt] = kSongFxScenesNone;
    }

    saveToWorkspace();
}

void SongFxScenes::onRemoved(uint8_t index)
{
    if (index >= SongListColumn::kSongSlotCount)
    {
        return;
    }

    for (uint8_t i = index; i < SongListColumn::kSongSlotCount - 1; i++)
    {
        externalRamSongFxScenesBank.songSlotScene[i] =
          externalRamSongFxScenesBank.songSlotScene[i + 1];
    }
    externalRamSongFxScenesBank.songSlotScene[SongListColumn::kSongSlotCount - 1] =
      kSongFxScenesNone;

    saveToWorkspace();
}

void SongFxScenes::clearAll()
{
    memset(externalRamSongFxScenesBank.songSlotScene,
           0,
           sizeof(externalRamSongFxScenesBank.songSlotScene));
    saveToWorkspace();
}

void SongFxScenes::applyScene(uint8_t sceneIndex)
{
    if (sceneIndex >= kSongFxScenesCount)
    {
        return;
    }

    PerformancePageHost& host = getPerformancePageHost();
    const strSongFxScene& scene = externalRamSongFxScenesBank.scenes[sceneIndex];
    for (uint8_t i = 0; i < kSongFxEntriesPerScene; i++)
    {
        const uint8_t fxId = scene.entries[i].fxId;
        if (fxId == 0 || fxId >= host.performanceFxCount())
        {
            continue;
        }
        host.applyPerformanceFx(fxId, scene.entries[i].value);
    }
}

void SongFxScenes::onSlotStart(uint8_t slot)
{
    const uint8_t scene = sceneForSlot(slot);
    if (scene == kSongFxScenesNone || scene > kSongFxScenesCount)
    {
        return;
    }
    applyScene(static_cast<uint8_t>(scene - 1));
}

bool SongFxScenes::canOpen(SongScreenHost& host) const
{
    (void)host;
    return true;
}

void SongFxScenes::onShow(SongScreenHost& host)
{
    const uint8_t scene = sceneForSlot(host.selectedSlot());
    if (scene >= 1 && scene <= kSongFxScenesCount)
    {
        editingScene = static_cast<uint8_t>(scene - 1);
    }
    else if (editingScene >= kSongFxScenesCount)
    {
        editingScene = 0;
    }
    focusedEntry = 0;
    focusedField = 0;

    strSongFxScene& bankScene = externalRamSongFxScenesBank.scenes[editingScene];
    if (bankScene.name[0] == '\0')
    {
        snprintf(bankScene.name,
                 sizeof(bankScene.name),
                 "FX %u",
                 static_cast<unsigned>(editingScene + 1));
        saveToWorkspace();
    }

    refreshEditorUi(host);
}

void SongFxScenes::selectEditingScene(SongScreenHost& host, uint8_t sceneIndex)
{
    if (sceneIndex >= kSongFxScenesCount)
    {
        return;
    }

    editingScene = sceneIndex;
    focusedEntry = 0;
    focusedField = 0;

    strSongFxScene& bankScene = externalRamSongFxScenesBank.scenes[editingScene];
    if (bankScene.name[0] == '\0')
    {
        snprintf(bankScene.name,
                 sizeof(bankScene.name),
                 "FX %u",
                 static_cast<unsigned>(editingScene + 1));
        saveToWorkspace();
    }

    refreshEditorUi(host);
}

void SongFxScenes::onHide(SongScreenHost& host)
{
    (void)host;
}

void SongFxScenes::onNavigate(SongScreenHost& host, int8_t delta)
{
    if (delta == 0)
    {
        return;
    }

    int16_t next = static_cast<int16_t>(focusedEntry) + delta;
    while (next < 0)
    {
        next += kSongFxEntriesPerScene;
    }
    while (next >= kSongFxEntriesPerScene)
    {
        next -= kSongFxEntriesPerScene;
    }
    focusedEntry = static_cast<uint8_t>(next);
    refreshEditorUi(host);
}

void SongFxScenes::onHorizontalNavigate(SongScreenHost& host, int8_t delta)
{
    if (delta == 0)
    {
        return;
    }

    focusedField = (focusedField == 0) ? 1 : 0;
    refreshEditorUi(host);
}

void SongFxScenes::onClearFocused(SongScreenHost& host)
{
    if (focusedEntry >= kSongFxEntriesPerScene)
    {
        return;
    }

    strSongFxEntry& e = externalRamSongFxScenesBank.scenes[editingScene].entries[focusedEntry];
    e.fxId = 0;
    e.value = 0;
    saveToWorkspace();
    refreshEditorUi(host);
}

void SongFxScenes::onBack(SongScreenHost& host)
{
    (void)host;
}

bool SongFxScenes::sceneHasEntries(uint8_t sceneIndex) const
{
    if (sceneIndex >= kSongFxScenesCount)
    {
        return false;
    }

    const strSongFxScene& scene = externalRamSongFxScenesBank.scenes[sceneIndex];
    for (uint8_t i = 0; i < kSongFxEntriesPerScene; i++)
    {
        if (scene.entries[i].fxId != 0)
        {
            return true;
        }
    }
    return false;
}

void SongFxScenes::formatEntryName(uint8_t entry, char* out, uint8_t outSize) const
{
    if (out == nullptr || outSize == 0 || entry >= kSongFxEntriesPerScene)
    {
        return;
    }

    const strSongFxEntry& e = externalRamSongFxScenesBank.scenes[editingScene].entries[entry];
    if (e.fxId == 0)
    {
        snprintf(out, outSize, "---");
        return;
    }

    PerformancePageHost& perfHost = getPerformancePageHost();
    const char* name = perfHost.performanceFxName(e.fxId);
    snprintf(out, outSize, "%s", name != nullptr && name[0] != '\0' ? name : "?");
}

void SongFxScenes::formatEntryValue(uint8_t entry, char* out, uint8_t outSize) const
{
    if (out == nullptr || outSize == 0 || entry >= kSongFxEntriesPerScene)
    {
        return;
    }

    const strSongFxEntry& e = externalRamSongFxScenesBank.scenes[editingScene].entries[entry];
    if (e.fxId == 0)
    {
        snprintf(out, outSize, "--");
        return;
    }

    snprintf(out, outSize, "%d", static_cast<int>(e.value));
}

void SongFxScenes::refreshEditorUi(SongScreenHost& host)
{
    static char titleBuf[24];
    snprintf(titleBuf,
             sizeof(titleBuf),
             "FX Scene %u",
             static_cast<unsigned>(editingScene + 1));
    host.setEditorTitle(titleBuf);

    static const char* const sceneLabels[kSongFxScenesCount] = {
        "1", "2", "3", "4", "5", "6", "7", "8"
    };

    for (uint8_t i = 0; i < kSongFxScenesCount; i++)
    {
        SongEditorLabelState state = SongEditorLabelState::Empty;
        if (i == editingScene)
        {
            state = SongEditorLabelState::Active;
        }
        else if (sceneHasEntries(i))
        {
            state = SongEditorLabelState::Filled;
        }
        host.setScreenLabel(i, sceneLabels[i], state, nullptr);
    }
    host.finishScreenLabels();

    static char nameBuf[kSongFxEntriesPerScene][24];
    static char valueBuf[kSongFxEntriesPerScene][12];
    host.setEditorListCount(kSongFxEntriesPerScene);
    for (uint8_t i = 0; i < kSongFxEntriesPerScene; i++)
    {
        formatEntryName(i, nameBuf[i], sizeof(nameBuf[i]));
        formatEntryValue(i, valueBuf[i], sizeof(valueBuf[i]));
        host.setEditorListRow(i, nameBuf[i], valueBuf[i]);
    }
    host.setEditorListFocus(focusedEntry, focusedField != 0);
    host.refreshEditor();
}

uint8_t SongFxScenes::nextFxId(uint8_t current, int8_t delta) const
{
    PerformancePageHost& host = getPerformancePageHost();
    const uint8_t count = host.performanceFxCount();
    // Skip fxId 0 (none); allow empty only via clear.
    const int16_t minId = 1;
    const int16_t maxId = static_cast<int16_t>(count > 0 ? count - 1 : 0);
    if (maxId < minId)
    {
        return current;
    }

    auto isNamed = [&](uint8_t id) {
        const char* name = host.performanceFxName(id);
        return name != nullptr && name[0] != '\0';
    };

    int16_t id = current;
    if (id < minId || id > maxId)
    {
        // First assignment from empty: find first named FX.
        for (int16_t i = minId; i <= maxId; i++)
        {
            if (isNamed(static_cast<uint8_t>(i)))
            {
                return static_cast<uint8_t>(i);
            }
        }
        return current;
    }

    if (delta == 0)
    {
        return current;
    }

    for (uint8_t guard = 0; guard < count; guard++)
    {
        id += delta > 0 ? 1 : -1;
        if (id < minId)
        {
            id = maxId;
        }
        if (id > maxId)
        {
            id = minId;
        }
        if (isNamed(static_cast<uint8_t>(id)))
        {
            return static_cast<uint8_t>(id);
        }
    }

    return current;
}

int16_t SongFxScenes::clampFxValue(uint8_t fxId, int16_t value) const
{
    PerformancePageHost& host = getPerformancePageHost();
    const int16_t minV = host.performanceFxMin(fxId);
    const int16_t maxV = host.performanceFxMax(fxId);
    if (value < minV)
    {
        return minV;
    }
    if (value > maxV)
    {
        return maxV;
    }
    return value;
}

void SongFxScenes::changeFocusedFxType(SongScreenHost& host, int8_t delta)
{
    if (delta == 0 || focusedEntry >= kSongFxEntriesPerScene)
    {
        return;
    }

    strSongFxEntry& e = externalRamSongFxScenesBank.scenes[editingScene].entries[focusedEntry];
    if (e.fxId == 0)
    {
        e.fxId = nextFxId(0, 1);
        e.value = 0;
    }
    else
    {
        e.fxId = nextFxId(e.fxId, delta);
        e.value = clampFxValue(e.fxId, e.value);
    }

    saveToWorkspace();
    refreshEditorUi(host);
}

void SongFxScenes::changeFocusedFxValue(SongScreenHost& host, int8_t delta)
{
    if (delta == 0 || focusedEntry >= kSongFxEntriesPerScene)
    {
        return;
    }

    strSongFxEntry& e = externalRamSongFxScenesBank.scenes[editingScene].entries[focusedEntry];
    if (e.fxId == 0)
    {
        // Empty row: +/- on value assigns an FX first, then stays on value.
        e.fxId = nextFxId(0, 1);
        e.value = 0;
        saveToWorkspace();
        refreshEditorUi(host);
        return;
    }

    e.value = clampFxValue(e.fxId, static_cast<int16_t>(e.value + delta));
    saveToWorkspace();
    refreshEditorUi(host);
}

void SongFxScenes::onEncoder(SongScreenHost& host, int8_t delta)
{
    if (focusedField == 0)
    {
        changeFocusedFxType(host, delta);
    }
    else
    {
        changeFocusedFxValue(host, delta);
    }
}

void SongFxScenes::onScreenButton(SongScreenHost& host, uint8_t button, uint8_t state)
{
    if (state != kPerformanceButtonPress || button >= kSongFxScenesCount)
    {
        return;
    }

    selectEditingScene(host, button);
}
