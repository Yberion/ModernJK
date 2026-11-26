/*
===========================================================================
Copyright (C) 1999 - 2005, Id Software, Inc.
Copyright (C) 2000 - 2013, Raven Software, Inc.
Copyright (C) 2001 - 2013, Activision, Inc.
Copyright (C) 2013 - 2015, OpenJK contributors
Copyright (C) 2026, ModernJK contributors

This file is part of the OpenJK source code.

OpenJK is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, see <http://www.gnu.org/licenses/>.
===========================================================================
*/

#pragma once

#include <optional>

#include <shared/sound/sound.h>

#include <ghoul2/G2.h>

inline constexpr int CGAME_API_VERSION = 6;

enum class uiMenuCommand_t; // ui_public.h
enum class fakeAscii_t;     // shared/qcommon/q_keycodes.h

inline constexpr int CMD_BACKUP = 256; // 2.56 s of commands at one per tick
inline constexpr int CMD_MASK = CMD_BACKUP - 1;
// allow a lot of command backups for very fast systems
// multiple commands may be combined into a single packet, so this
// needs to be larger than PACKET_BACKUP

inline constexpr int MAX_ENTITIES_IN_SNAPSHOT = 256;

// snapshots are a view of the server at a given time

// Snapshots are generated at regular time intervals by the server,
// but they may not be sent if a client's rate level is exceeded, or
// they may be dropped by the network.
typedef struct snapshot_s {
    snapFlags_t snapFlags; // SNAPFLAG_RATE_DELAYED, etc
    int ping;
    bool inputMarginValid; // the server measured how early it got the command of this snapshot's tick
    int inputMargin;       // that margin in ms (negative: the command came after its tick)

    int serverTime; // server time the message is valid for (in msec)

    byte areamask[MAX_MAP_AREA_BYTES]; // portalarea visibility bits

    playerState_t ps;  // complete information about the current player at this time
    playerState_t vps; // vehicle I'm riding's playerstate (if applicable) -rww

    int numEntities;                                  // all of the entities that need to be presented
    entityState_t entities[MAX_ENTITIES_IN_SNAPSHOT]; // at the time of this snapshot

    int numServerCommands;     // text based server commands to execute when this
    int serverCommandSequence; // snapshot becomes current
} snapshot_t;

enum class cgameEvent_t { CGAME_EVENT_NONE = 0, CGAME_EVENT_TEAMMENU, CGAME_EVENT_SCOREBOARD, CGAME_EVENT_EDITHUD };

typedef struct autoMapInput_s {
    float up;
    float down;
    float yaw;
    float pitch;
    bool goToDefaults;
} autoMapInput_t;

// CG_POINT_CONTENTS
typedef struct TCGPointContents_s {
    vec3_t mPoint;      // input
    int mPassEntityNum; // input
} TCGPointContents;

// CG_GET_BOLT_POS
typedef struct TCGGetBoltData_s {
    vec3_t mOrigin; // output
    vec3_t mAngles; // output
    vec3_t mScale;  // output
    int mEntityNum; // input
} TCGGetBoltData;

// CG_GET_LERP_ORIGIN
// CG_GET_LERP_ANGLES
// CG_GET_MODEL_SCALE
typedef struct TCGVectorData_s {
    int mEntityNum; // input
    vec3_t mPoint;  // output
} TCGVectorData;

// CG_TRACE/CG_G2TRACE
typedef struct TCGTrace_s {
    trace_t mResult;                   // output
    vec3_t mStart, mMins, mMaxs, mEnd; // input
    int mSkipNumber;                   // input
    contentsFlags_t mMask;             // input
} TCGTrace;

// CG_G2MARK
typedef struct TCGG2Mark_s {
    int shader;
    float size;
    vec3_t start, dir;
} TCGG2Mark;

// CG_INCOMING_CONSOLE_COMMAND
typedef struct TCGIncomingConsoleCommand_s {
    char conCommand[1024];
} TCGIncomingConsoleCommand;

// CG_FX_CAMERASHAKE
typedef struct TCGCameraShake_s {
    vec3_t mOrigin;   // input
    float mIntensity; // input
    int mRadius;      // input
    int mTime;        // input
} TCGCameraShake;

// CG_MISC_ENT
typedef struct TCGMiscEnt_s {
    char mModel[MAX_QPATH];          // input
    vec3_t mOrigin, mAngles, mScale; // input
} TCGMiscEnt;

typedef struct TCGPositionOnBolt_s {
    refEntity_t ent;   // output
    void* ghoul2;      // input
    int modelIndex;    // input
    int boltIndex;     // input
    vec3_t origin;     // input
    vec3_t angles;     // input
    vec3_t modelScale; // input
} TCGPositionOnBolt;

// ragdoll callback structs -rww
//  the type of a ragdoll callback, each with its structure in the shared buffer
enum class ragCallbackType_t {
    RAG_CALLBACK_DEBUGBOX = 1, // ragCallbackDebugBox_t
    RAG_CALLBACK_DEBUGLINE,    // ragCallbackDebugLine_t
    RAG_CALLBACK_BONESNAP,     // ragCallbackBoneSnap_t
    RAG_CALLBACK_BONEIMPACT,   // ragCallbackBoneImpact_t
    RAG_CALLBACK_BONEINSOLID,  // ragCallbackBoneInSolid_t
    RAG_CALLBACK_TRACELINE     // ragCallbackTraceLine_t
};

typedef struct ragCallbackDebugBox_s {
    vec3_t mins;
    vec3_t maxs;
    int duration;
} ragCallbackDebugBox_t;

typedef struct ragCallbackDebugLine_s {
    vec3_t start;
    vec3_t end;
    int time;
    int color;
    int radius;
} ragCallbackDebugLine_t;

typedef struct ragCallbackBoneSnap_s {
    char boneName[128]; // name of the bone in question
    int entNum;         // index of entity who owns the bone in question
} ragCallbackBoneSnap_t;

typedef struct ragCallbackBoneImpact_s {
    char boneName[128]; // name of the bone in question
    int entNum;         // index of entity who owns the bone in question
} ragCallbackBoneImpact_t;

typedef struct ragCallbackBoneInSolid_s {
    vec3_t bonePos; // world coordinate position of the bone
    int entNum;     // index of entity who owns the bone in question
    int solidCount; // higher the count, the longer we've been in solid (the worse off we are)
} ragCallbackBoneInSolid_t;

typedef struct ragCallbackTraceLine_s {
    trace_t tr;
    vec3_t start;
    vec3_t end;
    vec3_t mins;
    vec3_t maxs;
    int ignore;
    contentsFlags_t mask;
} ragCallbackTraceLine_t;

inline constexpr int MAX_CG_SHARED_BUFFER_SIZE = 2048;

typedef struct cgameImport_s {
    // common
    void (*Print)(printLevel_t level, lineEnd_t end, fmt::string_view fmt, fmt::format_args args);
    NORETURN_PTR void (*Error)(errorParm_t code, fmt::string_view fmt, fmt::format_args args);
    void (*SnapVector)(float* v);
    int (*MemoryRemaining)(void);
    void (*RegisterSharedMemory)(char* memory);
    void (*TrueMalloc)(void** ptr, int size);
    void (*TrueFree)(void** ptr);

    // timing
    int (*Milliseconds)(void);
    int (*RealTime)(qtime_t* qtime);
    void (*PrecisionTimerStart)(void** timer);
    int (*PrecisionTimerEnd)(void* timer);

    // cvar
    void (*Cvar_Register)(vmCvar_t* vmCvar, const char* varName, const char* defaultValue, cvarFlags_t flags);
    void (*Cvar_Set)(const char* var_name, const char* value);
    void (*Cvar_Update)(vmCvar_t* vmCvar);
    void (*Cvar_VariableStringBuffer)(const char* var_name, char* buffer, int bufsize);

    // command
    void (*AddCommand)(const char* cmd_name);
    int (*Cmd_Argc)(void);
    void (*Cmd_Args)(char* buffer, int bufferLength);
    void (*Cmd_Argv)(int arg, char* buffer, int bufferLength);
    void (*RemoveCommand)(const char* cmd_name);
    void (*SendClientCommand)(const char* cmd);
    void (*SendConsoleCommand)(const char* text);

    // filesystem
    void (*FS_Close)(fileHandle_t f);
    int (*FS_GetFileList)(const char* path, const char* extension, char* listbuf, int bufsize);
    int (*FS_Open)(const char* qpath, fileHandle_t* f, fsMode_t mode);
    int (*FS_Read)(void* buffer, int len, fileHandle_t f);
    int (*FS_Write)(const void* buffer, int len, fileHandle_t f);

    // screen
    void (*UpdateScreen)(void);

    // clip model
    clipHandle_t (*CM_InlineModel)(int index);
    void (*CM_LoadMap)(const char* mapname, bool subBSP);
    int (*CM_NumInlineModels)(void);
    contentsFlags_t (*CM_PointContents)(const vec3_t p, clipHandle_t model);
    int (*CM_RegisterTerrain)(const char* config);
    clipHandle_t (*CM_TempModel)(const vec3_t mins, const vec3_t maxs, int capsule);
    void (*CM_Trace)(trace_t* results, const vec3_t start, const vec3_t end, const vec3_t mins, const vec3_t maxs, clipHandle_t model,
                     contentsFlags_t brushmask, int capsule);
    contentsFlags_t (*CM_TransformedPointContents)(const vec3_t p, clipHandle_t model, const vec3_t origin, const vec3_t angles);
    void (*CM_TransformedTrace)(trace_t* results, const vec3_t start, const vec3_t end, const vec3_t mins, const vec3_t maxs, clipHandle_t model,
                                contentsFlags_t brushmask, const vec3_t origin, const vec3_t angles, int capsule);
    void (*RMG_Init)(int terrainID, const char* terrainInfo);

    // the sound's functions, the same object for the whole session
    sound::Api* sound;

    // renderer
    void (*R_AddAdditiveLightToScene)(const vec3_t org, float intensity, float r, float g, float b);
    void (*R_AddDecalToScene)(qhandle_t shader, const vec3_t origin, const vec3_t dir, float orientation, float r, float g, float b, float a, bool alphaFade,
                              float radius, bool temporary);
    void (*R_AddLightToScene)(const vec3_t org, float intensity, float r, float g, float b);
    void (*R_AddPolysToScene)(qhandle_t hShader, int numVerts, const polyVert_t* verts, int num);
    void (*R_AddRefEntityToScene)(const refEntity_t* re);
    unsigned int (*R_AnyLanguage_ReadCharFromString)(const char* psText, int* piAdvanceCount, bool* pbIsTrailingPunctuation);
    void (*R_AutomapElevationAdjustment)(float newHeight);
    void (*R_ClearDecals)(void);
    void (*R_ClearScene)(void);
    void (*R_DrawStretchPic)(float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t hShader);           // 0 = white
    void (*R_DrawRotatePic)(float x, float y, float w, float h, float s1, float t1, float s2, float t2, float a1, qhandle_t hShader);  // 0 = white
    void (*R_DrawRotatePic2)(float x, float y, float w, float h, float s1, float t1, float s2, float t2, float a1, qhandle_t hShader); // 0 = white
    void (*R_Font_DrawString)(int ox, int oy, const char* text, const float* rgba, const int setIndex, int iCharLimit, const float scale);
    int (*R_Font_HeightPixels)(const int iFontIndex, const float scale);
    int (*R_Font_StrLenChars)(const char* text);
    int (*R_Font_StrLenPixels)(const char* text, const int iFontIndex, const float scale);
    void (*R_GetBModelVerts)(int bmodelIndex, vec3_t* vec, vec3_t normal);
    float (*R_GetDistanceCull)(void);
    bool (*R_GetEntityToken)(char* buffer, int size);
    void (*R_GetLightStyle)(int style, color4ub_t color);
    void (*R_GetRealRes)(int* w, int* h);
    bool (*R_InitializeWireframeAutomap)(void);
    bool (*R_InPVS)(const vec3_t p1, const vec3_t p2, byte* mask);
    bool (*R_Language_IsAsian)(void);
    bool (*R_Language_UsesSpaces)(void);
    int (*R_LerpTag)(orientation_t* tag, qhandle_t model, int startFrame, int endFrame, float frac, const char* tagName);
    int (*R_LightForPoint)(vec3_t point, vec3_t ambientLight, vec3_t directedLight, vec3_t lightDir);
    void (*R_LoadWorld)(const char* name);
    int (*R_MarkFragments)(int numPoints, const vec3_t* points, const vec3_t projection, int maxPoints, vec3_t pointBuffer, int maxFragments,
                           markFragment_t* fragmentBuffer);
    void (*R_ModelBounds)(qhandle_t model, vec3_t mins, vec3_t maxs);
    qhandle_t (*R_RegisterFont)(const char* fontName);
    qhandle_t (*R_RegisterModel)(const char* name);
    qhandle_t (*R_RegisterShader)(const char* name);
    qhandle_t (*R_RegisterShaderNoMip)(const char* name);
    qhandle_t (*R_RegisterSkin)(const char* name);
    void (*R_RemapShader)(const char* oldShader, const char* newShader, const char* offsetTime);
    void (*R_RenderScene)(const refdef_t* fd);
    void (*R_SetColor)(const float* rgba); // nullptr = 1,1,1,1
    void (*R_SetLightStyle)(int style, int color);
    void (*R_SetRangedFog)(float range);
    void (*R_SetRefractionProperties)(float distortionAlpha, float distortionStretch, bool distortionPrePost, bool distortionNegate);
    void (*R_WorldEffectCommand)(const char* cmd);
    void (*RE_InitRendererTerrain)(const char* info);
    void (*WE_AddWeatherZone)(vec3_t mins, vec3_t maxs);

    // client
    void (*GetCurrentSnapshotNumber)(int* snapshotNumber, int* serverTime);
    int (*GetCurrentCmdNumber)(void);
    bool (*GetDefaultState)(int index, entityState_t* state);
    void (*GetGameState)(gameState_t* gs);
    void (*GetGlconfig)(glconfig_t* glconfig);
    bool (*GetServerCommand)(int serverCommandNumber);
    bool (*GetSnapshot)(int snapshotNumber, snapshot_t* snapshot);
    bool (*GetUserCmd)(int cmdNumber, usercmd_t* ucmd);
    void (*OpenUIMenu)(uiMenuCommand_t menu);
    void (*SetClientForceAngle)(int time, vec3_t angle);
    void (*SetUserCmdValue)(weapon_t stateValue, float sensitivityScale, float mPitchOverride, float mYawOverride, float mSensitivityOverride,
                            forcePowers_t fpSel, int invenSel, bool fighterControls);

    // keys
    keyCatchFlags_t (*Key_GetCatcher)(void);
    std::optional<fakeAscii_t> (*Key_GetKey)(const char* binding);
    bool (*Key_IsDown)(fakeAscii_t keynum);
    void (*Key_SetCatcher)(keyCatchFlags_t catcher);

    // preprocessor (botlib_export->PC_***)
    int (*PC_AddGlobalDefine)(char* string);
    int (*PC_FreeSource)(int handle);
    int (*PC_LoadGlobalDefines)(const char* filename);
    int (*PC_LoadSource)(const char* filename);
    int (*PC_ReadToken)(int handle, pc_token_t* pc_token);
    void (*PC_RemoveAllGlobalDefines)(void);
    int (*PC_SourceFileAndLine)(int handle, char* filename, int* line);

    // cinematics
    void (*CIN_DrawCinematic)(int handle);
    int (*CIN_PlayCinematic)(const char* arg, int x, int y, int w, int h, cinematicFlags_t systemBits);
    e_status (*CIN_RunCinematic)(int handle);
    void (*CIN_SetExtents)(int handle, int x, int y, int w, int h);
    e_status (*CIN_StopCinematic)(int handle);

    // FX
    void (*FX_AddLine)(vec3_t start, vec3_t end, float size1, float size2, float sizeParm, float alpha1, float alpha2, float alphaParm, vec3_t sRGB,
                       vec3_t eRGB, float rgbParm, int killTime, qhandle_t shader, fxPrimitiveFlags_t flags);
    int (*FX_RegisterEffect)(const char* file);
    void (*FX_PlayEffect)(const char* file, vec3_t org, vec3_t fwd, int vol, int rad);
    void (*FX_PlayEffectID)(int id, vec3_t org, vec3_t fwd, int vol, int rad, bool isPortal);
    void (*FX_PlayEntityEffectID)(int id, vec3_t org, matrix3_t axis, const int boltInfo, const int entNum, int vol, int rad);
    bool (*FX_PlayBoltedEffectID)(int id, vec3_t org, void* pGhoul2, const int boltNum, const int entNum, const int modelNum, int iLooptime, bool isRelative);
    void (*FX_AddScheduledEffects)(bool portal);
    int (*FX_InitSystem)(refdef_t* refdef);
    void (*FX_SetRefDef)(refdef_t* refdef);
    bool (*FX_FreeSystem)(void);
    void (*FX_AdjustTime)(int time);
    void (*FX_Draw2DEffects)(float screenXScale, float screenYScale);
    void (*FX_AddPoly)(addpolyArgStruct_t* p);
    void (*FX_AddBezier)(addbezierArgStruct_t* p);
    void (*FX_AddPrimitive)(effectTrailArgStruct_t* p);
    void (*FX_AddSprite)(addspriteArgStruct_t* p);
    void (*FX_AddElectricity)(addElectricityArgStruct_t* p);

    // stringed
    bool (*SE_GetStringTextString)(const char* text, char* buffer, int bufferLength);

    // roff
    bool (*ROFF_Clean)(void);
    void (*ROFF_UpdateEntities)(void);
    int (*ROFF_Cache)(char* file);
    bool (*ROFF_Play)(int entID, int roffID, bool doTranslation);
    bool (*ROFF_Purge_Ent)(int entID);

    // ghoul2
    void (*G2_ListModelSurfaces)(void* ghlInfo);
    void (*G2_ListModelBones)(void* ghlInfo, int frame);
    void (*G2_SetGhoul2ModelIndexes)(void* ghoul2, qhandle_t* modelList, qhandle_t* skinList);
    bool (*G2_HaveWeGhoul2Models)(void* ghoul2);
    bool (*G2API_GetBoltMatrix)(void* ghoul2, const int modelIndex, const int boltIndex, mdxaBone_t* matrix, const vec3_t angles, const vec3_t position,
                                const int frameNum, qhandle_t* modelList, vec3_t scale);
    bool (*G2API_GetBoltMatrix_NoReconstruct)(void* ghoul2, const int modelIndex, const int boltIndex, mdxaBone_t* matrix, const vec3_t angles,
                                              const vec3_t position, const int frameNum, qhandle_t* modelList, vec3_t scale);
    bool (*G2API_GetBoltMatrix_NoRecNoRot)(void* ghoul2, const int modelIndex, const int boltIndex, mdxaBone_t* matrix, const vec3_t angles,
                                           const vec3_t position, const int frameNum, qhandle_t* modelList, vec3_t scale);
    int (*G2API_InitGhoul2Model)(void** ghoul2Ptr, const char* fileName, int modelIndex, qhandle_t customSkin, qhandle_t customShader, int modelFlags,
                                 int lodBias);
    bool (*G2API_SetSkin)(void* ghoul2, int modelIndex, qhandle_t customSkin, qhandle_t renderSkin);
    void (*G2API_CollisionDetect)(CollisionRecord_t* collRecMap, void* ghoul2, const vec3_t angles, const vec3_t position, int frameNumber, int entNum,
                                  vec3_t rayStart, vec3_t rayEnd, vec3_t scale, int traceFlags, int useLod, float fRadius);
    void (*G2API_CollisionDetectCache)(CollisionRecord_t* collRecMap, void* ghoul2, const vec3_t angles, const vec3_t position, int frameNumber, int entNum,
                                       vec3_t rayStart, vec3_t rayEnd, vec3_t scale, int traceFlags, int useLod, float fRadius);
    void (*G2API_CleanGhoul2Models)(void** ghoul2Ptr);
    bool (*G2API_SetBoneAngles)(void* ghoul2, int modelIndex, const char* boneName, const vec3_t angles, const boneFlags_t flags, const orientations_t up,
                                const orientations_t right, const orientations_t forward, qhandle_t* modelList, int blendTime, int currentTime);
    bool (*G2API_SetBoneAnim)(void* ghoul2, const int modelIndex, const char* boneName, const int startFrame, const int endFrame, const boneFlags_t flags,
                              const float animSpeed, const int currentTime, const float setFrame, const int blendTime);
    bool (*G2API_GetBoneAnim)(void* ghoul2, const char* boneName, const int currentTime, float* currentFrame, int* startFrame, int* endFrame,
                              boneFlags_t* flags, float* animSpeed, int* modelList, const int modelIndex);
    bool (*G2API_GetBoneFrame)(void* ghoul2, const char* boneName, const int currentTime, float* currentFrame, int* modelList, const int modelIndex);
    void (*G2API_GetGLAName)(void* ghoul2, int modelIndex, char* fillBuf);
    int (*G2API_CopyGhoul2Instance)(void* g2From, void* g2To, int modelIndex);
    void (*G2API_CopySpecificGhoul2Model)(void* g2From, int modelFrom, void* g2To, int modelTo);
    void (*G2API_DuplicateGhoul2Instance)(void* g2From, void** g2To);
    bool (*G2API_HasGhoul2ModelOnIndex)(void* ghlInfo, int modelIndex);
    bool (*G2API_RemoveGhoul2Model)(void* ghlInfo, int modelIndex);
    bool (*G2API_SkinlessModel)(void* ghlInfo, int modelIndex);
    int (*G2API_GetNumGoreMarks)(void* ghlInfo, int modelIndex);
    void (*G2API_AddSkinGore)(void* ghlInfo, SSkinGoreData* gore);
    void (*G2API_ClearSkinGore)(void* ghlInfo);
    int (*G2API_Ghoul2Size)(void* ghlInfo);
    int (*G2API_AddBolt)(void* ghoul2, int modelIndex, const char* boneName);
    bool (*G2API_AttachEnt)(int* boltInfo, void* ghlInfoTo, int toBoltIndex, int entNum, int toModelNum);
    void (*G2API_SetBoltInfo)(void* ghoul2, int modelIndex, int boltInfo);
    bool (*G2API_SetRootSurface)(void* ghoul2, const int modelIndex, const char* surfaceName);
    bool (*G2API_SetSurfaceOnOff)(void* ghoul2, const char* surfaceName, const g2SurfaceFlags_t flags);
    bool (*G2API_SetNewOrigin)(void* ghoul2, const int boltIndex);
    bool (*G2API_DoesBoneExist)(void* ghoul2, int modelIndex, const char* boneName);
    std::optional<g2SurfaceFlags_t> (*G2API_GetSurfaceRenderStatus)(void* ghoul2, const int modelIndex, const char* surfaceName);
    int (*G2API_GetTime)(void);
    void (*G2API_SetTime)(int time, int clock);
    void (*G2API_AbsurdSmoothing)(void* ghoul2, bool status);
    void (*G2API_SetRagDoll)(void* ghoul2, sharedRagDollParams_t* params);
    void (*G2API_AnimateG2Models)(void* ghoul2, int time, sharedRagDollUpdateParams_t* params);
    bool (*G2API_RagPCJConstraint)(void* ghoul2, const char* boneName, vec3_t min, vec3_t max);
    bool (*G2API_RagPCJGradientSpeed)(void* ghoul2, const char* boneName, const float speed);
    bool (*G2API_RagEffectorGoal)(void* ghoul2, const char* boneName, vec3_t pos);
    bool (*G2API_GetRagBonePos)(void* ghoul2, const char* boneName, vec3_t pos, vec3_t entAngles, vec3_t entPos, vec3_t entScale);
    bool (*G2API_RagEffectorKick)(void* ghoul2, const char* boneName, vec3_t velocity);
    bool (*G2API_RagForceSolve)(void* ghoul2, bool force);
    bool (*G2API_SetBoneIKState)(void* ghoul2, int time, const char* boneName, sharedEIKMoveState ikState, sharedSetBoneIKStateParams_t* params);
    bool (*G2API_IKMove)(void* ghoul2, int time, sharedIKMoveParams_t* params);
    bool (*G2API_RemoveBone)(void* ghoul2, const char* boneName, int modelIndex);
    void (*G2API_AttachInstanceToEntNum)(void* ghoul2, int entityNum, bool server);
    void (*G2API_ClearAttachedInstance)(int entityNum);
    void (*G2API_CleanEntAttachments)(void);
    bool (*G2API_OverrideServer)(void* serverInstance);
    void (*G2API_GetSurfaceName)(void* ghoul2, int surfNumber, int modelIndex, char* fillBuf);

    struct {
        float (*R_Font_StrLenPixels)(const char* text, const int iFontIndex, const float scale);
    } ext;
} cgameImport_t;

typedef struct cgameExport_s {
    void (*Init)(int serverMessageNum, int serverCommandSequence, int clientNum);
    void (*Shutdown)(void);
    bool (*ConsoleCommand)(void);
    void (*DrawActiveFrame)(int serverTime, stereoFrame_t stereoView, bool demoPlayback);
    int (*CrosshairPlayer)(void);
    int (*LastAttacker)(void);
    void (*KeyEvent)(fakeAscii_t key, bool down);
    // a typed character, in the engine's one-byte set
    void (*CharEvent)(int character);
    // the pointer's place in the window, in pixels from its top left corner
    void (*MouseEvent)(int x, int y);
    void (*EventHandling)(cgameEvent_t type);
    contentsFlags_t (*PointContents)(void);
    void (*GetLerpOrigin)(void);
    void (*GetLerpData)(void);
    void (*Trace)(void);
    void (*G2Trace)(void);
    void (*G2Mark)(void);
    int (*RagCallback)(ragCallbackType_t callType);
    bool (*IncomingConsoleCommand)(void);
    bool (*NoUseableForce)(void);
    void (*GetOrigin)(int entID, vec3_t out);
    void (*GetAngles)(int entID, vec3_t out);
    trajectory_t* (*GetOriginTrajectory)(int entID);
    trajectory_t* (*GetAngleTrajectory)(int entID);
    void (*ROFF_NotetrackCallback)(int entID, const char* notetrack);
    void (*MapChange)(void);
    void (*AutomapInput)(void);
    void (*MiscEnt)(void);
    void (*CameraShake)(void);
} cgameExport_t;

// linking of cgame library
typedef cgameExport_t*(QDECL* GetCGameAPI_t)(int apiVersion, cgameImport_t* import);
