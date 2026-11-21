/*
===========================================================================
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

// Task Manager header file

#include <map>
#include <string>

#include "sequencer.h"
class CSequencer;

const int RUNAWAY_LIMIT = 256;

enum class taskReturn_t {
    TASK_RETURN_COMPLETE,
    TASK_RETURN_FAILED,
};

enum class taskResult_t {
    TASK_OK,
    TASK_FAILED,
};

// what MarkTask does to a task group
enum class taskMark_t {
    TASK_START,
    TASK_END,
};

// CTask

class CTask {
public:
    CTask();
    ~CTask();

    static CTask* Create(int GUID, CBlock* block);

    void Free(void);

    unsigned int GetTimeStamp(void) const { return m_timeStamp; }
    CBlock* GetBlock(void) const { return m_block; }
    int GetGUID(void) const { return m_id; }
    icarusToken_t GetID(void) const { return m_block->GetBlockID(); }

    void SetTimeStamp(unsigned int timeStamp) { m_timeStamp = timeStamp; }
    void SetBlock(CBlock* block) { m_block = block; }
    void SetGUID(int id) { m_id = id; }

protected:
    int m_id;
    unsigned int m_timeStamp;
    CBlock* m_block;
};

// CTaskGroup

class CTaskGroup {
public:
    typedef std::map<int, bool> taskCallback_m;

    CTaskGroup(void);
    ~CTaskGroup(void);

    void Init(void);

    taskResult_t Add(CTask* task);

    void SetGUID(int GUID);
    void SetParent(CTaskGroup* group) { m_parent = group; }

    bool Complete(void) const { return (m_numCompleted == (int)m_completedTasks.size()); }

    bool MarkTaskComplete(int id);

    CTaskGroup* GetParent(void) const { return m_parent; }
    int GetGUID(void) const { return m_GUID; }

    // protected:

    taskCallback_m m_completedTasks;

    CTaskGroup* m_parent;

    int m_numCompleted;
    int m_GUID;
};

// CTaskManager

class CTaskManager {

    typedef std::map<int, CTask*> taskID_m;
    typedef std::map<std::string, CTaskGroup*> taskGroupName_m;
    typedef std::map<int, CTaskGroup*> taskGroupID_m;
    typedef std::vector<CTaskGroup*> taskGroup_v;
    typedef std::list<CTask*> tasks_l;

public:
    CTaskManager();
    ~CTaskManager();

    static CTaskManager* Create(void);

    CBlock* GetCurrentTask(void);

    taskResult_t Init(CSequencer* owner);
    taskResult_t Free(void);

    int Flush(void);

    taskResult_t SetCommand(CBlock* block, pushPop_t type);
    taskResult_t Completed(int id);

    taskResult_t Update(void);
    bool IsRunning(void);

    CTaskGroup* AddTaskGroup(const char* name);
    CTaskGroup* GetTaskGroup(const char* name);
    CTaskGroup* GetTaskGroup(int id);

    taskResult_t MarkTask(int id, taskMark_t operation);
    CBlock* RecallTask(void);

    void Save(void);
    void Load(void);

protected:
    taskResult_t Go(void); // Heartbeat function called once per game frame
    taskResult_t CallbackCommand(CTask* task, taskReturn_t returnCode);

    inline bool Check(icarusToken_t targetID, CBlock* block, int memberNum);

    int GetVector(int entID, CBlock* block, int& memberNum, vector_t& value);
    int GetFloat(int entID, CBlock* block, int& memberNum, float& value);
    int GetToken(int entID, CBlock* block, int& memberNum, icarusToken_t& value);
    int Get(int entID, CBlock* block, int& memberNum, char** value);

    taskResult_t PushTask(CTask* task, pushPop_t flag);
    CTask* PopTask(pushPop_t flag);

    // Task functions
    taskResult_t Rotate(CTask* task);
    taskResult_t Remove(CTask* task);
    taskResult_t Camera(CTask* task);
    taskResult_t Print(CTask* task);
    taskResult_t Sound(CTask* task);
    taskResult_t Move(CTask* task);
    taskResult_t Kill(CTask* task);
    taskResult_t Set(CTask* task);
    taskResult_t Use(CTask* task);
    taskResult_t DeclareVariable(CTask* task);
    taskResult_t FreeVariable(CTask* task);
    taskResult_t Signal(CTask* task);
    taskResult_t Play(CTask* task);

    taskResult_t Wait(CTask* task, bool& completed);
    taskResult_t WaitSignal(CTask* task, bool& completed);

    int SaveCommand(CBlock* block);

    // Variables

    CSequencer* m_owner;
    int m_ownerID;

    CTaskGroup* m_curGroup;

    taskGroup_v m_taskGroups;
    tasks_l m_tasks;

    int m_GUID;
    int m_count;

    taskGroupName_m m_taskGroupNameMap;
    taskGroupID_m m_taskGroupIDMap;

    bool m_resident;

    // CTask	*m_waitTask;		//Global pointer to the current task that is waiting for callback completion
};
