/*
===========================================================================
Copyright (C) 2000 - 2013, Raven Software, Inc.
Copyright (C) 2001 - 2013, Activision, Inc.
Copyright (C) 2013 - 2015, OpenJK contributors

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

// Task Manager header file

#ifndef __TASK_MANAGER__
#define __TASK_MANAGER__

#include "../qcommon/q_shared.h"

const int RUNAWAY_LIMIT	= 256;

enum class taskReturn_t
{
	TASK_RETURN_COMPLETE,
	TASK_RETURN_FAILED,
};
using enum taskReturn_t;

enum class taskResult_t
{
	TASK_OK,
	TASK_FAILED,
};
using enum taskResult_t;

// what MarkTask does to a task group
enum class taskMark_t
{
	TASK_START,
	TASK_END,
};
using enum taskMark_t;

// CTask

class CTask
{
public:

	CTask();
	~CTask();

	static CTask *Create( int GUID, CBlock *block );

	void	Free( void );

	unsigned int	GetTimeStamp( void )	const	{	return m_timeStamp;				}
	CBlock	*GetBlock( void )		const	{	return m_block;					}
	int		GetGUID( void)			const	{	return m_id;					}
	icarusToken_t		GetID( void )			const	{	return m_block->GetBlockID();	}

	void	SetTimeStamp( unsigned int	timeStamp )		{	m_timeStamp = timeStamp;	}
	void	SetBlock( CBlock *block )			{	m_block = block;			}
	void	SetGUID( int id )					{	m_id = id;					}

	// Overloaded new operator.
	inline void *operator new( size_t size )
	{	// Allocate the memory.
		return IGameInterface::GetGame()->Malloc( size );
	}

	// Overloaded delete operator.
	inline void operator delete( void *pRawData )
	{	// Free the Memory.
		IGameInterface::GetGame()->Free( pRawData );
	}

protected:

	int		m_id;
	unsigned int	m_timeStamp;
	CBlock	*m_block;
};

// CTaskGroup

class CTaskGroup
{
public:

	typedef std::map < int, bool > taskCallback_m;

	CTaskGroup( void );
	~CTaskGroup( void );

	void Init( void );

	taskResult_t Add( CTask *task );

	void SetGUID( int GUID );
	void SetParent( CTaskGroup *group )	{	m_parent = group;	}

	bool Complete(void)		const { return ( m_numCompleted == m_completedTasks.size() ); }

	bool MarkTaskComplete( int id );

	CTaskGroup *GetParent( void )	const	{	return m_parent;	}
	int	GetGUID( void )				const	{	return m_GUID;		}

	// Overloaded new operator.
	static void *operator new( size_t size )
	{	// Allocate the memory.
		return IGameInterface::GetGame()->Malloc( size );
	}
	// Overloaded delete operator.
	static void operator delete( void *pRawData )
	{	// Free the Memory.
		IGameInterface::GetGame()->Free( pRawData );
	}

//protected:

	taskCallback_m	m_completedTasks;

	CTaskGroup		*m_parent;

	unsigned int	m_numCompleted;
	int				m_GUID;
};

// CTaskManager
class CSequencer;

class CTaskManager
{

	typedef	std::map < int, CTask * >			taskID_m;
	typedef std::map < std::string, CTaskGroup * >	taskGroupName_m;
	typedef std::map < int, CTaskGroup * >		taskGroupID_m;
	typedef std::vector < CTaskGroup * >			taskGroup_v;
	typedef std::list < CTask *>					tasks_l;

public:

	CTaskManager();
	~CTaskManager();

	int GetID();

	static CTaskManager *Create( void );

	CBlock *GetCurrentTask( void );

	taskResult_t Init( CSequencer *owner );
	taskResult_t	Free( void );

	int	Flush( void );

	taskResult_t	SetCommand( CBlock *block, CSequence::pushPop_t type, CIcarus* icarus );
	taskResult_t Completed( int id );

	taskResult_t Update( CIcarus* icarus );
	int IsRunning( void ) const { return(!m_tasks.empty()); };
	bool IsResident( void ) const { return m_resident;};

	CTaskGroup *AddTaskGroup( const char *name , CIcarus* icarus);
	CTaskGroup *GetTaskGroup( const char *name, CIcarus* icarus);
	CTaskGroup *GetTaskGroup( int id, CIcarus* icarus );

	taskResult_t MarkTask( int id, taskMark_t operation, CIcarus* icarus );
	CBlock *RecallTask( void );

	void Save();
	void Load( CIcarus* icarus );

	// Overloaded new operator.
	inline void* operator new( size_t size )
	{	// Allocate the memory.
		return IGameInterface::GetGame()->Malloc( size );
	}

	// Overloaded delete operator.
	inline void operator delete( void *pRawData )
	{	// Free the Memory.
		IGameInterface::GetGame()->Free( pRawData );
	}

protected:

	taskResult_t	Go( CIcarus* icarus );	//Heartbeat function called once per game frame
	taskResult_t CallbackCommand( CTask *task, taskReturn_t returnCode, CIcarus* icarus );

	inline bool Check( icarusToken_t targetID, CBlock *block, int memberNum ) const;

	int GetVector( int entID, CBlock *block, int &memberNum, vec3_t &value, CIcarus* icarus );
	int GetFloat( int entID, CBlock *block, int &memberNum, float &value, CIcarus* icarus );
	int GetToken( int entID, CBlock *block, int &memberNum, icarusToken_t &value, CIcarus* icarus );
	int Get( int entID, CBlock *block, int &memberNum, const char **value, CIcarus* icarus );

	taskResult_t	PushTask( CTask *task, CSequence::pushPop_t flag );
	CTask *PopTask( CSequence::pushPop_t flag );

	// Task functions
	taskResult_t Rotate( CTask *task, CIcarus* icarus );
	taskResult_t Remove( CTask *task , CIcarus* icarus);
	taskResult_t Camera( CTask *task, CIcarus* icarus );
	taskResult_t Print( CTask *task , CIcarus* icarus);
	taskResult_t Sound( CTask *task, CIcarus* icarus );
	taskResult_t Move( CTask *task , CIcarus* icarus);
	taskResult_t Kill( CTask *task , CIcarus* icarus);
	taskResult_t Set( CTask *task, CIcarus* icarus );
	taskResult_t Use( CTask *task , CIcarus* icarus);
	taskResult_t DeclareVariable( CTask *task , CIcarus* icarus);
	taskResult_t FreeVariable( CTask *task, CIcarus* icarus );
	taskResult_t Signal( CTask *task , CIcarus* icarus);
	taskResult_t Play( CTask *task , CIcarus* icarus);

	taskResult_t Wait( CTask *task, bool &completed, CIcarus* icarus );
	taskResult_t WaitSignal( CTask *task, bool &completed, CIcarus* icarus);

	int	SaveCommand( CBlock *block );

	// Variables

	CSequencer				*m_owner;
	int						m_ownerID;

	CTaskGroup				*m_curGroup;

	taskGroup_v				m_taskGroups;
	tasks_l					m_tasks;

	int						m_GUID;
	int						m_count;

	taskGroupName_m			m_taskGroupNameMap;
	taskGroupID_m			m_taskGroupIDMap;

	bool					m_resident;

	int						m_id;

	//CTask	*m_waitTask;		//Global pointer to the current task that is waiting for callback completion
};

#endif	//__TASK_MANAGER__
