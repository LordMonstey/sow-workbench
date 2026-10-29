#pragma once

#include <windows.h>

namespace Monstey::Monstey_AppInternal
{

constexpr UINT Monstey_ReplyMessage = WM_APP + 31, Monstey_WorkerStopped = WM_APP + 32;
constexpr wchar_t Monstey_MainClass[] = L"Monstey_SoWNativeWorkbench";
constexpr wchar_t Monstey_EditorClass[] = L"Monstey_SoWPropertyEditor";
constexpr wchar_t Monstey_InspectorClass[] = L"Monstey_SoWRecordProperties";
constexpr int Monstey_Open = 101, Monstey_New = 102, Monstey_Load = 103, Monstey_Save = 104, Monstey_Build = 105,
              Monstey_Undo = 106, Monstey_Redo = 107, Monstey_Revert = 108, Monstey_Clone = 109, Monstey_Rename = 110,
              Monstey_Delete = 111, Monstey_Add = 112, Monstey_Replace = 113, Monstey_Extract = 114,
              Monstey_Folder = 115, Monstey_Dependencies = 116, Monstey_Schema = 117, Monstey_Exit = 118,
              Monstey_About = 119, Monstey_Filter = 120, Monstey_Search = 121, Monstey_Previous = 122,
              Monstey_Next = 123, Monstey_Tabs = 124, Monstey_Categories = 125, Monstey_Records = 126,
              Monstey_Fields = 127, Monstey_ConfigureMenu = 130, Monstey_CatalogMenu = 131, Monstey_MountsMenu = 132,
              Monstey_EditorApply = 201, Monstey_EditorCancel = 202, Monstey_EditorValue = 203,
              Monstey_EditorBrowse = 204, Monstey_EditorFormat = 205, Monstey_Structure = 133,
              Monstey_CreateCategory = 134, Monstey_CreateRecord = 135, Monstey_AddField = 136,
              Monstey_DeleteField = 137, Monstey_MoveFieldUp = 138, Monstey_MoveFieldDown = 139, Monstey_Effects = 140,
              Monstey_FxCreate = 141, Monstey_FxRename = 142, Monstey_FxDuplicate = 143, Monstey_FxDelete = 144,
              Monstey_FxDuration = 145, Monstey_FxTrackCreate = 146, Monstey_FxTrackRename = 147,
              Monstey_FxTrackDelete = 148, Monstey_FxTrackUp = 149, Monstey_FxTrackDown = 150,
              Monstey_FxKeyCreate = 151, Monstey_FxKeyDuplicate = 152, Monstey_FxKeyRename = 153,
              Monstey_FxKeyDelete = 154, Monstey_FxKeyLink = 155, Monstey_FxKeyUnlink = 156, Monstey_FxKeyUp = 157,
              Monstey_FxKeyDown = 158, Monstey_FxKeyRetime = 159, Monstey_FxReconcile = 160, Monstey_Sockets = 161,
              Monstey_SocketCreate = 162, Monstey_SocketEdit = 163, Monstey_SocketCopy = 164,
              Monstey_SocketRemove = 165, Monstey_SocketRig = 166, Monstey_SocketRigClear = 167,
              Monstey_SocketPrev = 168, Monstey_SocketNext = 169, Monstey_FxLibrary = 170, Monstey_FxGroupCreate = 171,
              Monstey_FxGroupRename = 172, Monstey_FxGroupMove = 173, Monstey_FxGroupDelete = 174,
              Monstey_FxPrefabCreate = 175, Monstey_FxPrefabFromKey = 176, Monstey_FxPrefabDuplicate = 177,
              Monstey_FxPrefabRename = 178, Monstey_FxPrefabMove = 179, Monstey_FxPrefabParent = 180,
              Monstey_FxPrefabAssign = 181, Monstey_FxPrefabActive = 182, Monstey_FxPrefabChild = 183,
              Monstey_FxPrefabOverride = 184, Monstey_FxPrefabInherit = 185, Monstey_FxPrefabDetach = 186,
              Monstey_FxPrefabDelete = 187, Monstey_FxPrefabSync = 188, Monstey_Envelope = 189, Monstey_Extras = 190,
              Monstey_ExtraUsage = 191, Monstey_FxNested = 192, Monstey_FxNestedInspect = 193,
              Monstey_FxNestedSlot = 194, Monstey_FxNestedOptions = 195, Monstey_FxNestedBack = 196,
              Monstey_FxAttachment = 197, Monstey_Animations = 210, Monstey_AnimationHeld = 211,
              Monstey_AnimationTiming = 212, Monstey_AnimationEvents = 213, Monstey_AnimationEventAdd = 214,
              Monstey_AnimationEventEdit = 215, Monstey_AnimationEventRemove = 216, Monstey_AnimationSample = 217,
              Monstey_AnimationExportAll = 218, Monstey_AnimationSkeleton = 219, Monstey_AnimationChanges = 220,
              Monstey_AnimationEventClear = 221, Monstey_ModelPreview = 222, Monstey_Materials = 223,
              Monstey_MaterialEdit = 224, Monstey_MaterialRaw = 225, Monstey_MaterialChanges = 226,
              Monstey_TexturePreview = 227, Monstey_NativeProfileChoose = 228, Monstey_NativeProfileClear = 229,
              Monstey_NativeBanksMenu = 230, Monstey_NativeClassMenu = 231, Monstey_DatabaseRecordsMenu = 232,
              Monstey_NativeDatabaseTools = 233, Monstey_InspectStorage = 234;

} // namespace Monstey::Monstey_AppInternal
