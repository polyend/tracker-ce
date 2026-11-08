#include "FxListScenes.h"

#include "ModuleSidecar.h"
#include "Platform.h"

#include <cr_section_macros.h>
#include <cstdio>
#include <cstring>

__NOINIT(EXTERNAL_RAM) static strFxListScenesFile externalRamFxListScenesBank;

FxListScenes fxListScenes;

static const PatternStepFxInfo kFxListScenePatternStepFx = {
    kFxListScenePatternStepFxId,
    'N',
    "FX Scene",
    "FX Scene",
    "Apply FX List scene 1-8.",
    1,
    kFxListScenesCount,
    1,
};

const PatternStepFxInfo* FxListScenes::patternStepFx(uint8_t index) const
{
    return index == 0 ? &kFxListScenePatternStepFx : nullptr;
}

bool FxListScenes::onPatternStepFx(uint8_t typeId, uint8_t value)
{
    if (typeId != kFxListScenePatternStepFxId)
    {
        return false;
    }

    if (value < 1 || value > kFxListScenesCount)
    {
        return true;
    }

    applyScene(static_cast<uint8_t>(value - 1));
    return true;
}

void FxListScenes::init(FileSystem* fileSystem)
{
    this->fileSystem = fileSystem;
    clearBank();
}

void FxListScenes::onWorkspaceClear()
{
    clearBank();
}

void FxListScenes::onWorkspaceLoad()
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

void FxListScenes::onProjectOpen(const char* projectPath, const char* projectName)
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

void FxListScenes::onProjectSave(const char* projectPath, const char* projectName)
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

void FxListScenes::clearBank()
{
    memset(&externalRamFxListScenesBank, 0, sizeof(externalRamFxListScenesBank));
    externalRamFxListScenesBank.magic[0] = 'F';
    externalRamFxListScenesBank.magic[1] = 'X';
    externalRamFxListScenesBank.magic[2] = 'L';
    externalRamFxListScenesBank.magic[3] = 'S';
    externalRamFxListScenesBank.version = kFxListScenesFileVersion;
    externalRamFxListScenesBank.count = kFxListScenesCount;
    editingScene = 0;
    focusedEntry = 0;
}

bool FxListScenes::validateHeader() const
{
    return externalRamFxListScenesBank.magic[0] == 'F' &&
           externalRamFxListScenesBank.magic[1] == 'X' &&
           externalRamFxListScenesBank.magic[2] == 'L' &&
           externalRamFxListScenesBank.magic[3] == 'S' &&
           externalRamFxListScenesBank.version == kFxListScenesFileVersion &&
           externalRamFxListScenesBank.count == kFxListScenesCount;
}

void FxListScenes::clampSongSlotScenes()
{
    for (uint8_t i = 0; i < SongListColumn::kSongSlotCount; i++)
    {
        if (externalRamFxListScenesBank.songSlotScene[i] > kFxListScenesCount)
        {
            externalRamFxListScenesBank.songSlotScene[i] = kFxListScenesNone;
        }
    }
    externalRamFxListScenesBank.songSlotScene[SongListColumn::kSongSlotCount] = 0;
}

bool FxListScenes::readFile(const char* path)
{
    if (!ModuleSidecar::read(
          fileSystem, path, &externalRamFxListScenesBank, sizeof(externalRamFxListScenesBank)))
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

void FxListScenes::writeFile(const char* path)
{
    externalRamFxListScenesBank.magic[0] = 'F';
    externalRamFxListScenesBank.magic[1] = 'X';
    externalRamFxListScenesBank.magic[2] = 'L';
    externalRamFxListScenesBank.magic[3] = 'S';
    externalRamFxListScenesBank.version = kFxListScenesFileVersion;
    externalRamFxListScenesBank.count = kFxListScenesCount;
    clampSongSlotScenes();

    ModuleSidecar::write(
      fileSystem, path, &externalRamFxListScenesBank, sizeof(externalRamFxListScenesBank));
}

void FxListScenes::saveToWorkspace()
{
    char path[ModuleSidecar::kMaxPathLength];
    ModuleSidecar::workspacePath(path, sizeof(path), id());
    writeFile(path);
}

uint8_t FxListScenes::sceneForSlot(uint8_t slot) const
{
    if (slot >= SongListColumn::kSongSlotCount)
    {
        return kFxListScenesNone;
    }
    return externalRamFxListScenesBank.songSlotScene[slot];
}

void FxListScenes::assignSlot(uint8_t slot, uint8_t scene)
{
    if (slot >= SongListColumn::kSongSlotCount)
    {
        return;
    }

    if (scene > kFxListScenesCount)
    {
        scene = kFxListScenesNone;
    }

    if (externalRamFxListScenesBank.songSlotScene[slot] == scene)
    {
        return;
    }

    externalRamFxListScenesBank.songSlotScene[slot] = scene;
    saveToWorkspace();
}

void FxListScenes::formatCell(uint8_t slot, char* out, uint8_t outSize) const
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
    if (value == kFxListScenesNone)
    {
        snprintf(out, outSize, "-");
        return;
    }

    snprintf(out, outSize, "%u", static_cast<unsigned>(value));
}

void FxListScenes::onEncoder(SongScreenHost& host, uint8_t slot, int8_t delta)
{
    (void)host;
    if (slot >= SongListColumn::kSongSlotCount || delta == 0)
    {
        return;
    }

    int16_t value = sceneForSlot(slot);
    value += delta;

    const int16_t maxValue = kFxListScenesCount;
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

void FxListScenes::onClear(SongScreenHost& host, uint8_t slot)
{
    (void)host;
    assignSlot(slot, kFxListScenesNone);
}

void FxListScenes::onInserted(uint8_t index, bool duplicate)
{
    if (index >= SongListColumn::kSongSlotCount - 1)
    {
        return;
    }

    for (int i = SongListColumn::kSongSlotCount - 1; i > index; i--)
    {
        externalRamFxListScenesBank.songSlotScene[i] =
          externalRamFxListScenesBank.songSlotScene[i - 1];
    }

    const uint8_t insertAt = static_cast<uint8_t>(index + 1);
    if (duplicate)
    {
        externalRamFxListScenesBank.songSlotScene[insertAt] =
          externalRamFxListScenesBank.songSlotScene[index];
    }
    else
    {
        externalRamFxListScenesBank.songSlotScene[insertAt] = kFxListScenesNone;
    }

    saveToWorkspace();
}

void FxListScenes::onRemoved(uint8_t index)
{
    if (index >= SongListColumn::kSongSlotCount)
    {
        return;
    }

    for (uint8_t i = index; i < SongListColumn::kSongSlotCount - 1; i++)
    {
        externalRamFxListScenesBank.songSlotScene[i] =
          externalRamFxListScenesBank.songSlotScene[i + 1];
    }
    externalRamFxListScenesBank.songSlotScene[SongListColumn::kSongSlotCount - 1] =
      kFxListScenesNone;

    saveToWorkspace();
}

void FxListScenes::clearAll()
{
    memset(externalRamFxListScenesBank.songSlotScene,
           0,
           sizeof(externalRamFxListScenesBank.songSlotScene));
    saveToWorkspace();
}

void FxListScenes::applyScene(uint8_t sceneIndex)
{
    if (sceneIndex >= kFxListScenesCount)
    {
        return;
    }

    PerformancePageHost& host = getPerformancePageHost();
    const strFxListScene& scene = externalRamFxListScenesBank.scenes[sceneIndex];
    for (uint8_t i = 0; i < kFxListEntriesPerScene; i++)
    {
        const uint8_t fxId = scene.entries[i].fxId;
        if (fxId == 0 || fxId >= host.performanceFxCount())
        {
            continue;
        }
        host.applyPerformanceFx(fxId, scene.entries[i].value);
    }
}

void FxListScenes::onSlotStart(uint8_t slot)
{
    const uint8_t scene = sceneForSlot(slot);
    if (scene == kFxListScenesNone || scene > kFxListScenesCount)
    {
        return;
    }
    applyScene(static_cast<uint8_t>(scene - 1));
}

bool FxListScenes::canOpen(SongScreenHost& host) const
{
    (void)host;
    return true;
}

void FxListScenes::onShow(SongScreenHost& host)
{
    const uint8_t scene = sceneForSlot(host.selectedSlot());
    if (scene >= 1 && scene <= kFxListScenesCount)
    {
        editingScene = static_cast<uint8_t>(scene - 1);
    }
    else if (editingScene >= kFxListScenesCount)
    {
        editingScene = 0;
    }
    focusedEntry = 0;
    focusedField = 0;

    strFxListScene& bankScene = externalRamFxListScenesBank.scenes[editingScene];
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

void FxListScenes::selectEditingScene(SongScreenHost& host, uint8_t sceneIndex)
{
    if (sceneIndex >= kFxListScenesCount)
    {
        return;
    }

    editingScene = sceneIndex;
    focusedEntry = 0;
    focusedField = 0;

    strFxListScene& bankScene = externalRamFxListScenesBank.scenes[editingScene];
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

void FxListScenes::onHide(SongScreenHost& host)
{
    (void)host;
}

void FxListScenes::onNavigate(SongScreenHost& host, int8_t delta)
{
    if (delta == 0)
    {
        return;
    }

    int16_t next = static_cast<int16_t>(focusedEntry) + delta;
    while (next < 0)
    {
        next += kFxListEntriesPerScene;
    }
    while (next >= kFxListEntriesPerScene)
    {
        next -= kFxListEntriesPerScene;
    }
    focusedEntry = static_cast<uint8_t>(next);
    refreshEditorUi(host);
}

void FxListScenes::onHorizontalNavigate(SongScreenHost& host, int8_t delta)
{
    if (delta == 0)
    {
        return;
    }

    focusedField = (focusedField == 0) ? 1 : 0;
    refreshEditorUi(host);
}

void FxListScenes::onClearFocused(SongScreenHost& host)
{
    if (focusedEntry >= kFxListEntriesPerScene)
    {
        return;
    }

    strFxListEntry& e = externalRamFxListScenesBank.scenes[editingScene].entries[focusedEntry];
    e.fxId = 0;
    e.value = 0;
    saveToWorkspace();
    refreshEditorUi(host);
}

void FxListScenes::onBack(SongScreenHost& host)
{
    (void)host;
}

bool FxListScenes::sceneHasEntries(uint8_t sceneIndex) const
{
    if (sceneIndex >= kFxListScenesCount)
    {
        return false;
    }

    const strFxListScene& scene = externalRamFxListScenesBank.scenes[sceneIndex];
    for (uint8_t i = 0; i < kFxListEntriesPerScene; i++)
    {
        if (scene.entries[i].fxId != 0)
        {
            return true;
        }
    }
    return false;
}

void FxListScenes::formatEntryName(uint8_t entry, char* out, uint8_t outSize) const
{
    if (out == nullptr || outSize == 0 || entry >= kFxListEntriesPerScene)
    {
        return;
    }

    const strFxListEntry& e = externalRamFxListScenesBank.scenes[editingScene].entries[entry];
    if (e.fxId == 0)
    {
        snprintf(out, outSize, "---");
        return;
    }

    PerformancePageHost& perfHost = getPerformancePageHost();
    const char* name = perfHost.performanceFxName(e.fxId);
    snprintf(out, outSize, "%s", name != nullptr && name[0] != '\0' ? name : "?");
}

void FxListScenes::formatEntryValue(uint8_t entry, char* out, uint8_t outSize) const
{
    if (out == nullptr || outSize == 0 || entry >= kFxListEntriesPerScene)
    {
        return;
    }

    const strFxListEntry& e = externalRamFxListScenesBank.scenes[editingScene].entries[entry];
    if (e.fxId == 0)
    {
        snprintf(out, outSize, "--");
        return;
    }

    snprintf(out, outSize, "%d", static_cast<int>(e.value));
}

void FxListScenes::refreshEditorUi(SongScreenHost& host)
{
    static char titleBuf[24];
    snprintf(titleBuf,
             sizeof(titleBuf),
             "FX List %u",
             static_cast<unsigned>(editingScene + 1));
    host.setEditorTitle(titleBuf);

    static const char* const sceneLabels[kFxListScenesCount] = {
        "1", "2", "3", "4", "5", "6", "7", "8"
    };

    for (uint8_t i = 0; i < kFxListScenesCount; i++)
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

    static char nameBuf[kFxListEntriesPerScene][24];
    static char valueBuf[kFxListEntriesPerScene][12];
    host.setEditorListCount(kFxListEntriesPerScene);
    for (uint8_t i = 0; i < kFxListEntriesPerScene; i++)
    {
        formatEntryName(i, nameBuf[i], sizeof(nameBuf[i]));
        formatEntryValue(i, valueBuf[i], sizeof(valueBuf[i]));
        host.setEditorListRow(i, nameBuf[i], valueBuf[i]);
    }
    host.setEditorListFocus(focusedEntry, focusedField != 0);
    host.refreshEditor();
}

uint8_t FxListScenes::nextFxId(uint8_t current, int8_t delta) const
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

int16_t FxListScenes::clampFxValue(uint8_t fxId, int16_t value) const
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

void FxListScenes::changeFocusedFxType(SongScreenHost& host, int8_t delta)
{
    if (delta == 0 || focusedEntry >= kFxListEntriesPerScene)
    {
        return;
    }

    strFxListEntry& e = externalRamFxListScenesBank.scenes[editingScene].entries[focusedEntry];
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

void FxListScenes::changeFocusedFxValue(SongScreenHost& host, int8_t delta)
{
    if (delta == 0 || focusedEntry >= kFxListEntriesPerScene)
    {
        return;
    }

    strFxListEntry& e = externalRamFxListScenesBank.scenes[editingScene].entries[focusedEntry];
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

void FxListScenes::onEncoder(SongScreenHost& host, int8_t delta)
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

void FxListScenes::onScreenButton(SongScreenHost& host, uint8_t button, uint8_t state)
{
    if (state != kPerformanceButtonPress || button >= kFxListScenesCount)
    {
        return;
    }

    selectEditingScene(host, button);
}
