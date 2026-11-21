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

// Task Manager
//
//	-- jweier


// this include must remain at the top of every Icarus CPP file
#include "icarus.h"
#include "g_local.h"


#include <assert.h>
#include "../code/qcommon/ojk_saved_game_helper.h"

#define ICARUS_VALIDATE(a) if ( a == false ) return taskResult_t::TASK_FAILED;
// in the functions that return true or false
#define ICARUS_VALIDATE_BOOL(a) if ( a == false ) return false;

/*
=================================================

CTask

=================================================
*/

CTask::CTask( void )
{
}

CTask::~CTask( void )
{
}

CTask *CTask::Create( int GUID, CBlock *block )
{
	CTask *task = new CTask;

	//TODO: Emit warning
	assert( task );
	if ( task == nullptr )
		return nullptr;

	task->SetTimeStamp( 0 );
	task->SetBlock( block );
	task->SetGUID( GUID );

	return task;
}

/*
-------------------------
Free
-------------------------
*/

void CTask::Free( void )
{
	//NOTENOTE: The block is not consumed by the task, it is the sequencer's job to clean blocks up
	delete this;
}

/*
=================================================

CTaskGroup

=================================================
*/

CTaskGroup::CTaskGroup( void )
{
	Init();

	m_GUID		= 0;
	m_parent	= nullptr;
}

CTaskGroup::~CTaskGroup( void )
{
	m_completedTasks.clear();
}

/*
-------------------------
SetGUID
-------------------------
*/

void CTaskGroup::SetGUID( int GUID )
{
	m_GUID = GUID;
}

/*
-------------------------
Init
-------------------------
*/

void CTaskGroup::Init( void )
{
	m_completedTasks.clear();

	m_numCompleted	= 0;
	m_parent		= nullptr;
}

/*
-------------------------
Add
-------------------------
*/

taskResult_t CTaskGroup::Add( CTask *task )
{
	m_completedTasks[ task->GetGUID() ] = false;
	return taskResult_t::TASK_OK;
}

/*
-------------------------
MarkTaskComplete
-------------------------
*/

bool CTaskGroup::MarkTaskComplete( int id )
{
	if ( (m_completedTasks.find( id )) != m_completedTasks.end() )
	{
		m_completedTasks[ id ] = true;
		m_numCompleted++;

		return true;
	}

	return false;
}

/*
=================================================

CTaskManager

=================================================
*/

CTaskManager::CTaskManager( void )
{
}

CTaskManager::~CTaskManager( void )
{
}

/*
-------------------------
Create
-------------------------
*/

CTaskManager *CTaskManager::Create( void )
{
	return new CTaskManager;
}

/*
-------------------------
Init
-------------------------
*/

taskResult_t	CTaskManager::Init( CSequencer *owner )
{
	//TODO: Emit warning
	if ( owner == nullptr )
		return taskResult_t::TASK_FAILED;

	m_tasks.clear();
	m_owner		= owner;
	m_ownerID	= owner->GetOwnerID();
	m_curGroup	= nullptr;
	m_GUID		= 0;
	m_resident	= false;

	return taskResult_t::TASK_OK;
}

/*
-------------------------
Free
-------------------------
*/

taskResult_t CTaskManager::Free( void )
{
	taskGroup_v::iterator	gi;
	tasks_l::iterator		ti;

	//Clear out all pending tasks
	for ( ti = m_tasks.begin(); ti != m_tasks.end(); ++ti )
	{
		(*ti)->Free();
	}

	m_tasks.clear();

	//Clear out all taskGroups
	for ( gi = m_taskGroups.begin(); gi != m_taskGroups.end(); ++gi )
	{
		delete (*gi);
	}

	m_taskGroups.clear();
	m_taskGroupNameMap.clear();
	m_taskGroupIDMap.clear();

	return taskResult_t::TASK_OK;
}

/*
-------------------------
Flush
-------------------------
*/

int CTaskManager::Flush( void )
{
	//FIXME: Rewrite

	return true;
}

/*
-------------------------
AddTaskGroup
-------------------------
*/

CTaskGroup *CTaskManager::AddTaskGroup( const char *name )
{
	CTaskGroup *group;

	//Collect any garbage
	taskGroupName_m::iterator	tgni;
	tgni = m_taskGroupNameMap.find( name );

	if ( tgni != m_taskGroupNameMap.end() )
	{
		group = (*tgni).second;

		//Clear it and just move on
		group->Init();

		return group;
	}

	//Allocate a new one
	group = new CTaskGroup;;

	//TODO: Emit warning
	assert( group );
	if ( group == nullptr )
	{
		(m_owner->GetInterface())->I_DPrintf( WL_e::WL_ERROR, "Unable to allocate task group \"%s\"\n", name );
		return nullptr;
	}

	//Setup the internal information
	group->SetGUID( m_GUID++ );

	//Add it to the list and associate it for retrieval later
	m_taskGroups.insert( m_taskGroups.end(), group );
	m_taskGroupNameMap[ name ] = group;
	m_taskGroupIDMap[ group->GetGUID() ] = group;

	return group;
}

/*
-------------------------
GetTaskGroup
-------------------------
*/

CTaskGroup *CTaskManager::GetTaskGroup( const char *name )
{
	taskGroupName_m::iterator	tgi;

	tgi = m_taskGroupNameMap.find( name );

	if ( tgi == m_taskGroupNameMap.end() )
	{
		(m_owner->GetInterface())->I_DPrintf( WL_e::WL_WARNING, "Could not find task group \"%s\"\n", name );
		return nullptr;
	}

	return (*tgi).second;
}

CTaskGroup *CTaskManager::GetTaskGroup( int id )
{
	taskGroupID_m::iterator	tgi;

	tgi = m_taskGroupIDMap.find( id );

	if ( tgi == m_taskGroupIDMap.end() )
	{
		(m_owner->GetInterface())->I_DPrintf( WL_e::WL_WARNING, "Could not find task group \"%d\"\n", id );
		return nullptr;
	}

	return (*tgi).second;
}

/*
-------------------------
Update
-------------------------
*/

taskResult_t CTaskManager::Update( void )
{
	if ( Any( g_entities[m_ownerID].svFlags & svFlags_t::SVF_ICARUS_FREEZE ) )
	{
		return taskResult_t::TASK_FAILED;
	}
	m_count = 0;	//Needed for runaway init
	m_resident = true;

	taskResult_t returnVal = Go();

	m_resident = false;

	return returnVal;
}

/*
-------------------------
IsRunning
-------------------------
*/

bool CTaskManager::IsRunning( void )
{
	return !m_tasks.empty();
}
/*
-------------------------
Check
-------------------------
*/

inline bool CTaskManager::Check( icarusToken_t targetID, CBlock *block, int memberNum )
{
	if ( (block->GetMember( memberNum ))->GetID() == targetID )
		return true;

	return false;
}

/*
-------------------------
GetFloat
-------------------------
*/

int CTaskManager::GetFloat( int entID, CBlock *block, int &memberNum, float &value )
{
	char	*name;
	icarusToken_t	type;

	//See if this is a get() command replacement
	if ( Check( icarusToken_t::ID_GET, block, memberNum ) )
	{
		//Update the member past the header id
		memberNum++;

		//get( TYPE, NAME )
		type = block->GetMemberToken( memberNum++ );
		name = (char *) block->GetMemberData( memberNum++ );

		//TODO: Emit warning
		if ( type != icarusToken_t::TK_FLOAT )
		{
			(m_owner->GetInterface())->I_DPrintf( WL_e::WL_ERROR, "Get() call tried to return a non-FLOAT parameter!\n" );
			return false;
		}

		return (m_owner->GetInterface())->I_GetFloat( entID, type, name, &value );
	}

	//Look for a Q_flrand(0.0f, 1.0f) inline call
	if ( Check( icarusToken_t::ID_RANDOM, block, memberNum ) )
	{
		float	min, max;

		memberNum++;

		min	= *(float *) block->GetMemberData( memberNum++ );
		max	= *(float *) block->GetMemberData( memberNum++ );

		value = (m_owner->GetInterface())->I_Random( min, max );

		return true;
	}

	//Look for a tag() inline call
	if ( Check( icarusToken_t::ID_TAG, block, memberNum ) )
	{
		(m_owner->GetInterface())->I_DPrintf( WL_e::WL_WARNING, "Invalid use of \"tag\" inline.  Not a valid replacement for type FLOAT\n" );
		return false;
	}

	CBlockMember	*bm	= block->GetMember( memberNum );

	if ( bm->GetID() == icarusToken_t::TK_INT )
	{
		value = (float) (*(int *) block->GetMemberData( memberNum++ ));
	}
	else if ( bm->GetID() == icarusToken_t::TK_FLOAT )
	{
		value = *(float *) block->GetMemberData( memberNum++ );
	}
	else
	{
		assert(0);
		(m_owner->GetInterface())->I_DPrintf( WL_e::WL_WARNING, "Unexpected value; expected type FLOAT\n" );
		return false;
	}

	return true;
}

/*
-------------------------
GetToken
-------------------------
*/

// a float parameter that holds a token (the lookup of a tag, the function of a camera)
int CTaskManager::GetToken( int entID, CBlock *block, int &memberNum, icarusToken_t &value )
{
	float	fValue;

	if ( GetFloat( entID, block, memberNum, fValue ) == false )
		return false;

	value = static_cast<icarusToken_t>( (int) fValue );
	return true;
}

/*
-------------------------
GetVector
-------------------------
*/

int CTaskManager::GetVector( int entID, CBlock *block, int &memberNum, vector_t &value )
{
	char	*name;
	icarusToken_t	type;
	int		i;

	//See if this is a get() command replacement
	if ( Check( icarusToken_t::ID_GET, block, memberNum ) )
	{
		//Update the member past the header id
		memberNum++;

		//get( TYPE, NAME )
		type = block->GetMemberToken( memberNum++ );
		name = (char *) block->GetMemberData( memberNum++ );

		//TODO: Emit warning
		if ( type != icarusToken_t::TK_VECTOR )
		{
			(m_owner->GetInterface())->I_DPrintf( WL_e::WL_ERROR, "Get() call tried to return a non-VECTOR parameter!\n" );
		}

		return (m_owner->GetInterface())->I_GetVector( entID, type, name, value );
	}

	//Look for a Q_flrand(0.0f, 1.0f) inline call
	if ( Check( icarusToken_t::ID_RANDOM, block, memberNum ) )
	{
		float	min, max;

		memberNum++;

		min	= *(float *) block->GetMemberData( memberNum++ );
		max	= *(float *) block->GetMemberData( memberNum++ );

		for ( i = 0; i < 3; i++ )
		{
			value[i] = (float) (m_owner->GetInterface())->I_Random( min, max );	//FIXME: Just truncating it for now.. should be fine though
		}

		return true;
	}

	//Look for a tag() inline call
	if ( Check( icarusToken_t::ID_TAG, block, memberNum ) )
	{
		const char	*tagName;
		icarusToken_t	tagLookup;

		memberNum++;
		ICARUS_VALIDATE_BOOL ( Get( entID, block, memberNum, &tagName ) );
		ICARUS_VALIDATE_BOOL ( GetToken( entID, block, memberNum, tagLookup ) );

		if ( (m_owner->GetInterface())->I_GetTag( entID, tagName, tagLookup, value ) == false)
		{
			(m_owner->GetInterface())->I_DPrintf( WL_e::WL_ERROR, "Unable to find tag \"%s\"!\n", tagName );
			assert(0);
			return false;
		}

		return true;
	}

	//Check for a real vector here
	type = block->GetMemberToken( memberNum );

	if ( type != icarusToken_t::TK_VECTOR )
	{
//		(m_owner->GetInterface())->I_DPrintf( WL_e::WL_WARNING, "Unexpected value; expected type VECTOR\n" );
		return false;
	}

	memberNum++;

	for ( i = 0; i < 3; i++ )
	{
		if ( GetFloat( entID, block, memberNum, value[i] ) == false )
			return false;
	}

	return true;
}

/*
-------------------------
Get
-------------------------
*/

int CTaskManager::Get( int entID, CBlock *block, int &memberNum, const char **value )
{
	static	char	tempBuffer[128];	//FIXME: EEEK!
	vector_t		vector;
	const char			*name, *tagName;
	icarusToken_t	tagLookup;
	icarusToken_t	type;

	//Look for a get() inline call
	if ( Check( icarusToken_t::ID_GET, block, memberNum ) )
	{
		//Update the member past the header id
		memberNum++;

		//get( TYPE, NAME )
		type = block->GetMemberToken( memberNum++ );
		name = (char *) block->GetMemberData( memberNum++ );

		//Format the return properly
		//FIXME: This is probably doing double formatting in certain cases...
		//FIXME: STRING MANAGEMENT NEEDS TO BE IMPLEMENTED, MY CURRENT SOLUTION IS NOT ACCEPTABLE!!
		switch ( type )
		{
		case icarusToken_t::TK_STRING:
			if ( ( m_owner->GetInterface())->I_GetString( entID, type, name, value ) == false )
			{
				(m_owner->GetInterface())->I_DPrintf( WL_e::WL_ERROR, "Get() parameter \"%s\" could not be found!\n", name );
				return false;
			}

			return true;
			break;

		case icarusToken_t::TK_FLOAT:
			{
				float	temp;

				if ( (m_owner->GetInterface())->I_GetFloat( entID, type, name, &temp ) == false )
				{
					(m_owner->GetInterface())->I_DPrintf( WL_e::WL_ERROR, "Get() parameter \"%s\" could not be found!\n", name );
					return false;
				}

				Com_sprintf( tempBuffer, sizeof( tempBuffer ), "%f", temp );
				*value = (char *) tempBuffer;
			}

			return true;
			break;

		case icarusToken_t::TK_VECTOR:
			{
				vector_t	vval;

				if ( (m_owner->GetInterface())->I_GetVector( entID, type, name, vval )  == false )
				{
					(m_owner->GetInterface())->I_DPrintf( WL_e::WL_ERROR, "Get() parameter \"%s\" could not be found!\n", name );
					return false;
				}

				Com_sprintf( tempBuffer, sizeof( tempBuffer ), "%f %f %f", vval[0], vval[1], vval[2] );
				*value = (char *) tempBuffer;
			}

			return true;
			break;

		default:
			(m_owner->GetInterface())->I_DPrintf( WL_e::WL_ERROR, "Get() call tried to return an unknown type!\n" );
			return false;
			break;
		}
	}

	//Look for a Q_flrand(0.0f, 1.0f) inline call
	if ( Check( icarusToken_t::ID_RANDOM, block, memberNum ) )
	{
		float	min, max, ret;

		memberNum++;

		min	= *(float *) block->GetMemberData( memberNum++ );
		max	= *(float *) block->GetMemberData( memberNum++ );

		ret = ( m_owner->GetInterface())->I_Random( min, max );

		Com_sprintf( tempBuffer, sizeof( tempBuffer ), "%f", ret );
		*value = (char *) tempBuffer;

		return true;
	}

	//Look for a tag() inline call
	if ( Check( icarusToken_t::ID_TAG, block, memberNum ) )
	{
		memberNum++;
		ICARUS_VALIDATE_BOOL ( Get( entID, block, memberNum, &tagName ) );
		ICARUS_VALIDATE_BOOL ( GetToken( entID, block, memberNum, tagLookup ) );

		if ( ( m_owner->GetInterface())->I_GetTag( entID, tagName, tagLookup, vector ) == false)
		{
			(m_owner->GetInterface())->I_DPrintf( WL_e::WL_ERROR, "Unable to find tag \"%s\"!\n", tagName );
			assert(0 && "Unable to find tag");
			return false;
		}

		Com_sprintf( tempBuffer, sizeof( tempBuffer ), "%f %f %f", vector[0], vector[1], vector[2] );
		*value = (char *) tempBuffer;

		return true;
	}

	//Get an actual piece of data

	CBlockMember	*bm	= block->GetMember( memberNum );

	if ( bm->GetID() == icarusToken_t::TK_INT )
	{
		float fval = (float) (*(int *) block->GetMemberData( memberNum++ ));
		Com_sprintf( tempBuffer, sizeof( tempBuffer ), "%f", fval );
		*value = (char *) tempBuffer;

		return true;
	}
	else if ( bm->GetID() == icarusToken_t::TK_FLOAT )
	{
		float fval = *(float *) block->GetMemberData( memberNum++ );
		Com_sprintf( tempBuffer, sizeof( tempBuffer ), "%f", fval );
		*value = (char *) tempBuffer;

		return true;
	}
	else if ( bm->GetID() == icarusToken_t::TK_VECTOR )
	{
		vector_t	vval;

		memberNum++;

		for ( int i = 0; i < 3; i++ )
		{
			if ( GetFloat( entID, block, memberNum, vval[i] ) == false )
				return false;
		}

		Com_sprintf( tempBuffer, sizeof( tempBuffer ), "%f %f %f", vval[0], vval[1], vval[2] );
		*value = (char *) tempBuffer;

		return true;
	}
	else if ( ( bm->GetID() == icarusToken_t::TK_STRING ) || ( bm->GetID() == icarusToken_t::TK_IDENTIFIER ) )
	{
		*value = (char *) block->GetMemberData( memberNum++ );

		return true;
	}

	//TODO: Emit warning
	assert( 0 );
	(m_owner->GetInterface())->I_DPrintf( WL_e::WL_WARNING, "Unexpected value; expected type STRING\n" );

	return false;
}

/*
-------------------------
Go
-------------------------
*/

taskResult_t	CTaskManager::Go( void )
{
	CTask	*task = nullptr;
	bool	completed = false;

	//Check for run away scripts
	if ( m_count++ > RUNAWAY_LIMIT )
	{
		assert(0);
		(m_owner->GetInterface())->I_DPrintf( WL_e::WL_ERROR, "Runaway loop detected!\n" );
		return taskResult_t::TASK_FAILED;
	}

	//If there are tasks to complete, do so
	if ( m_tasks.empty() == false )
	{
		//Get the next task
		task = PopTask( pushPop_t::POP_BACK );

		assert( task );
		if ( task == nullptr )
		{
			(m_owner->GetInterface())->I_DPrintf( WL_e::WL_ERROR, "Invalid task found in Go()!\n" );
			return taskResult_t::TASK_FAILED;
		}

		//If this hasn't been stamped, do so
		if ( task->GetTimeStamp() == 0 )
			task->SetTimeStamp( ( m_owner->GetInterface())->I_GetTime() );

		//Switch and call the proper function
		switch( task->GetID() )
		{
		case icarusToken_t::ID_WAIT:

			Wait( task, completed );

			//Push it to consider it again on the next frame if not complete
			if ( completed == false )
			{
				PushTask( task, pushPop_t::PUSH_BACK );
				return taskResult_t::TASK_OK;
			}

			Completed( task->GetGUID() );

			break;

		case icarusToken_t::ID_WAITSIGNAL:

			WaitSignal( task, completed );

			//Push it to consider it again on the next frame if not complete
			if ( completed == false )
			{
				PushTask( task, pushPop_t::PUSH_BACK );
				return taskResult_t::TASK_OK;
			}

			Completed( task->GetGUID() );

			break;

		case icarusToken_t::ID_PRINT:	//print( STRING )
			Print( task );
			break;

		case icarusToken_t::ID_SOUND:	//sound( name )
			Sound( task );
			break;

		case icarusToken_t::ID_MOVE:	//move ( Eorientations::ORIGIN, ANGLES, DURATION )
			Move( task );
			break;

		case icarusToken_t::ID_ROTATE:	//rotate( ANGLES, DURATION )
			Rotate( task );
			break;

		case icarusToken_t::ID_KILL:	//kill( NAME )
			Kill( task );
			break;

		case icarusToken_t::ID_REMOVE:	//remove( NAME )
			Remove( task );
			break;

		case icarusToken_t::ID_CAMERA:	//camera( ? )
			Camera( task );
			break;

		case icarusToken_t::ID_SET:	//set( NAME, ? )
			Set( task );
			break;

		case icarusToken_t::ID_USE:	//use( NAME )
			Use( task );
			break;

		case icarusToken_t::ID_DECLARE://declare( TYPE, NAME )
			DeclareVariable( task );
			break;

		case icarusToken_t::ID_FREE:	//free( NAME )
			FreeVariable( task );
			break;

		case icarusToken_t::ID_SIGNAL:	//signal( NAME )
			Signal( task );
			break;

		case icarusToken_t::ID_PLAY:	//play ( NAME )
			Play( task );
			break;

		default:
			assert(0);
			task->Free();
			(m_owner->GetInterface())->I_DPrintf( WL_e::WL_ERROR, "Found unknown task type!\n" );
			return taskResult_t::TASK_FAILED;
			break;
		}

		//Pump the sequencer for another task
		CallbackCommand( task, taskReturn_t::TASK_RETURN_COMPLETE );

		task->Free();
	}

	//FIXME: A command surge limiter could be implemented at this point to be sure a script doesn't
	//		 execute too many commands in one cycle.  This may, however, cause timing errors to surface.

	return taskResult_t::TASK_OK;
}

/*
-------------------------
SetCommand
-------------------------
*/

taskResult_t	CTaskManager::SetCommand( CBlock *command, pushPop_t type )
{
	CTask	*task = CTask::Create( m_GUID++, command );

	//If this is part of a task group, add it in
	if ( m_curGroup )
	{
		m_curGroup->Add( task );
	}

	//TODO: Emit warning
	assert( task );
	if ( task == nullptr )
	{
		(m_owner->GetInterface())->I_DPrintf( WL_e::WL_ERROR, "Unable to allocate new task!\n" );
		return taskResult_t::TASK_FAILED;
	}

	PushTask( task, type );

	return taskResult_t::TASK_OK;
}

/*
-------------------------
MarkTask
-------------------------
*/

taskResult_t CTaskManager::MarkTask( int id, taskMark_t operation )
{
	CTaskGroup *group	= GetTaskGroup( id );

	assert( group );

	if ( group == nullptr )
		return taskResult_t::TASK_FAILED;

	if ( operation == taskMark_t::TASK_START )
	{
		//Reset all the completion information
		group->Init();

		group->SetParent( m_curGroup );
		m_curGroup = group;
	}
	else if ( operation == taskMark_t::TASK_END )
	{
		assert( m_curGroup );
		if ( m_curGroup == nullptr )
			return taskResult_t::TASK_FAILED;

		m_curGroup = m_curGroup->GetParent();
	}

#ifdef _DEBUG
	else
	{
		assert(0);
	}
#endif

	return taskResult_t::TASK_OK;
}

/*
-------------------------
Completed
-------------------------
*/

taskResult_t CTaskManager::Completed( int id )
{
	taskGroup_v::iterator	tgi;

	//Mark the task as completed
	for ( tgi = m_taskGroups.begin(); tgi != m_taskGroups.end(); ++tgi )
	{
		//If this returns true, then the task was marked properly
		if ( (*tgi)->MarkTaskComplete( id ) )
			break;
	}

	return taskResult_t::TASK_OK;
}

/*
-------------------------
CallbackCommand
-------------------------
*/

taskResult_t	CTaskManager::CallbackCommand( CTask *task, taskReturn_t returnCode )
{
	if ( m_owner->Callback( this, task->GetBlock(), returnCode ) == sequencerResult_t::SEQ_OK )
		return Go( );

	assert(0);

	(m_owner->GetInterface())->I_DPrintf( WL_e::WL_ERROR, "Command callback failure!\n" );
	return taskResult_t::TASK_FAILED;
}

/*
-------------------------
RecallTask
-------------------------
*/

CBlock *CTaskManager::RecallTask( void )
{
	CTask	*task;

	task = PopTask( pushPop_t::POP_BACK );

	if ( task )
	{
	// fixed 2/12/2 to free the task that has been popped (called from sequencer Recall)
		CBlock* retBlock = task->GetBlock();
		task->Free();

		return retBlock;
	//	return task->GetBlock();
	}

	return nullptr;
}

/*
-------------------------
PushTask
-------------------------
*/

taskResult_t	CTaskManager::PushTask( CTask *task, pushPop_t flag )
{
	assert( (flag == pushPop_t::PUSH_FRONT) || (flag == pushPop_t::PUSH_BACK) );

	switch ( flag )
	{
	case pushPop_t::PUSH_FRONT:
		m_tasks.insert(m_tasks.begin(), task);

		return taskResult_t::TASK_OK;
		break;

	case pushPop_t::PUSH_BACK:
		m_tasks.insert(m_tasks.end(), task);

		return taskResult_t::TASK_OK;
		break;

	default:
		break;
	}

	//Invalid flag
	return taskResult_t::TASK_FAILED;
}

/*
-------------------------
PopTask
-------------------------
*/

CTask *CTaskManager::PopTask( pushPop_t flag )
{
	CTask	*task;

	assert( (flag == pushPop_t::POP_FRONT) || (flag == pushPop_t::POP_BACK) );

	if ( m_tasks.empty() )
		return nullptr;

	switch ( flag )
	{
	case pushPop_t::POP_FRONT:
		task = m_tasks.front();
		m_tasks.pop_front();

		return task;
		break;

	case pushPop_t::POP_BACK:
		task = m_tasks.back();
		m_tasks.pop_back();

		return task;
		break;

	default:
		break;
	}

	//Invalid flag
	return nullptr;
}

/*
-------------------------
GetCurrentTask
-------------------------
*/

CBlock *CTaskManager::GetCurrentTask( void )
{
	CTask *task = PopTask( pushPop_t::POP_BACK );

	if ( task == nullptr )
		return nullptr;
// fixed 2/12/2 to free the task that has been popped (called from sequencer Interrupt)
	CBlock* retBlock = task->GetBlock();
	task->Free();

	return retBlock;
//	return task->GetBlock();
}

/*
=================================================

  Task Functions

=================================================
*/

taskResult_t CTaskManager::Wait( CTask *task, bool &completed  )
{
	CBlockMember	*bm;
	CBlock			*block = task->GetBlock();
	const char			*sVal;
	float			dwtime;
	int				memberNum = 0;

	completed = false;

	bm = block->GetMember( 0 );

	//Check if this is a task completion wait
	if ( bm->GetID() == icarusToken_t::TK_STRING )
	{
		ICARUS_VALIDATE( Get( m_ownerID, block, memberNum, &sVal ) );

		if ( task->GetTimeStamp() == (m_owner->GetInterface())->I_GetTime() )
		{
			//Print out the debug info
			(m_owner->GetInterface())->I_DPrintf( WL_e::WL_DEBUG, "%4d wait(\"%s\"); [%d]", m_ownerID, sVal, task->GetTimeStamp() );
		}

		CTaskGroup	*group = GetTaskGroup( sVal );

		if ( group == nullptr )
		{
			//TODO: Emit warning
			completed = false;
			return taskResult_t::TASK_FAILED;
		}

		completed = group->Complete();
	}
	else	//Otherwise it's a time completion wait
	{
		if ( Check( icarusToken_t::ID_RANDOM, block, memberNum ) )
		{//get it random only the first time
			float	min, max;

			dwtime = *(float *) block->GetMemberData( memberNum++ );
			if ( dwtime == Q3_INFINITE )
			{//we have not evaluated this random yet
				min	= *(float *) block->GetMemberData( memberNum++ );
				max	= *(float *) block->GetMemberData( memberNum++ );

				dwtime = (m_owner->GetInterface())->I_Random( min, max );

				//store the result in the first member
				bm->SetData( &dwtime, sizeof( dwtime ) );
			}
		}
		else
		{
			ICARUS_VALIDATE( GetFloat( m_ownerID, block, memberNum, dwtime ) );
		}

		if ( task->GetTimeStamp() == (m_owner->GetInterface())->I_GetTime() )
		{
			//Print out the debug info
			(m_owner->GetInterface())->I_DPrintf( WL_e::WL_DEBUG, "%4d wait( %d ); [%d]", m_ownerID, (int) dwtime, task->GetTimeStamp() );
		}

		if ( (task->GetTimeStamp() + dwtime) < ((m_owner->GetInterface())->I_GetTime()) )
		{
			completed = true;
			memberNum = 0;
			if ( Check( icarusToken_t::ID_RANDOM, block, memberNum ) )
			{//set the data back to 0 so it will be re-randomized next time
				dwtime = Q3_INFINITE;
				bm->SetData( &dwtime, sizeof( dwtime ) );
			}
		}
	}

	return taskResult_t::TASK_OK;
}

/*
-------------------------
WaitSignal
-------------------------
*/

taskResult_t CTaskManager::WaitSignal( CTask *task, bool &completed  )
{
	CBlock	*block = task->GetBlock();
	const char	*sVal;
	int		memberNum = 0;

	completed = false;

	ICARUS_VALIDATE( Get( m_ownerID, block, memberNum, &sVal ) );

	if ( task->GetTimeStamp() == (m_owner->GetInterface())->I_GetTime() )
	{
		//Print out the debug info
		(m_owner->GetInterface())->I_DPrintf( WL_e::WL_DEBUG, "%4d waitsignal(\"%s\"); [%d]", m_ownerID, sVal, task->GetTimeStamp() );
	}

	if ( (m_owner->GetOwner())->CheckSignal( sVal ) )
	{
		completed = true;
		(m_owner->GetOwner())->ClearSignal( sVal );
	}

	return taskResult_t::TASK_OK;
}

/*
-------------------------
Print
-------------------------
*/

taskResult_t CTaskManager::Print( CTask *task )
{
	CBlock	*block = task->GetBlock();
	const char	*sVal;
	int		memberNum = 0;

	ICARUS_VALIDATE( Get( m_ownerID, block, memberNum, &sVal ) );

	(m_owner->GetInterface())->I_DPrintf( WL_e::WL_DEBUG, "%4d print(\"%s\"); [%d]", m_ownerID, sVal, task->GetTimeStamp() );

	(m_owner->GetInterface())->I_CenterPrint( "%s", sVal );

	Completed( task->GetGUID() );

	return taskResult_t::TASK_OK;
}

/*
-------------------------
Sound
-------------------------
*/

taskResult_t CTaskManager::Sound( CTask *task )
{
	CBlock	*block = task->GetBlock();
	const char	*sVal, *sVal2;
	int		memberNum = 0;

	ICARUS_VALIDATE( Get( m_ownerID, block, memberNum, &sVal ) );
	ICARUS_VALIDATE( Get( m_ownerID, block, memberNum, &sVal2 ) );

	(m_owner->GetInterface())->I_DPrintf( WL_e::WL_DEBUG, "%4d sound(\"%s\", \"%s\"); [%d]", m_ownerID, sVal, sVal2, task->GetTimeStamp() );

	//Only instantly complete if the user has requested it
	if( (m_owner->GetInterface())->I_PlaySound( task->GetGUID(), m_ownerID, sVal2, sVal ) )
		Completed( task->GetGUID() );

	return taskResult_t::TASK_OK;
}

/*
-------------------------
Rotate
-------------------------
*/

taskResult_t CTaskManager::Rotate( CTask *task )
{
	vector_t	vector;
	CBlock		*block = task->GetBlock();
	const char		*tagName;
	icarusToken_t	tagLookup;
	float		duration;
	int			memberNum = 0;

	//Check for a tag reference
	if ( Check( icarusToken_t::ID_TAG, block, memberNum ) )
	{
		memberNum++;

		ICARUS_VALIDATE( Get( m_ownerID, block, memberNum, &tagName ) );
		ICARUS_VALIDATE( GetToken( m_ownerID, block, memberNum, tagLookup ) );

		if ( (m_owner->GetInterface())->I_GetTag( m_ownerID, tagName, tagLookup, vector ) == false )
		{
			//TODO: Emit warning
			(m_owner->GetInterface())->I_DPrintf( WL_e::WL_ERROR, "Unable to find tag \"%s\"!\n", tagName );
			assert(0);
			return taskResult_t::TASK_FAILED;
		}
	}
	else
	{
		//Get a normal vector
		ICARUS_VALIDATE( GetVector( m_ownerID, block, memberNum, vector ) );
	}

	//Find the duration
	ICARUS_VALIDATE( GetFloat( m_ownerID, block, memberNum, duration ) );

	(m_owner->GetInterface())->I_DPrintf( WL_e::WL_DEBUG, "%4d rotate( <%f,%f,%f>, %d); [%d]", m_ownerID, vector[0], vector[1], vector[2], (int) duration, task->GetTimeStamp() );
	(m_owner->GetInterface())->I_Lerp2Angles( task->GetGUID(), m_ownerID, vector, duration );

	return taskResult_t::TASK_OK;
}

/*
-------------------------
Remove
-------------------------
*/

taskResult_t CTaskManager::Remove( CTask *task )
{
	CBlock	*block = task->GetBlock();
	const char	*sVal;
	int		memberNum = 0;

	ICARUS_VALIDATE( Get( m_ownerID, block, memberNum, &sVal ) );

	(m_owner->GetInterface())->I_DPrintf( WL_e::WL_DEBUG, "%4d remove(\"%s\"); [%d]", m_ownerID, sVal, task->GetTimeStamp() );
	(m_owner->GetInterface())->I_Remove( m_ownerID, sVal );

	Completed( task->GetGUID() );

	return taskResult_t::TASK_OK;
}

/*
-------------------------
Camera
-------------------------
*/

taskResult_t CTaskManager::Camera( CTask *task )
{
	interface_export_t	*ie = ( m_owner->GetInterface() );
	CBlock		*block = task->GetBlock();
	vector_t	vector, vector2;
	icarusToken_t	type;
	float		fVal, fVal2, fVal3;
	const char		*sVal;
	int			memberNum = 0;

	//Get the camera function type
	ICARUS_VALIDATE( GetToken( m_ownerID, block, memberNum, type ) );

	switch ( type )
	{
	case icarusToken_t::TYPE_PAN:

		ICARUS_VALIDATE( GetVector( m_ownerID, block, memberNum, vector ) );
		ICARUS_VALIDATE( GetVector( m_ownerID, block, memberNum, vector2 ) );

		ICARUS_VALIDATE( GetFloat( m_ownerID, block, memberNum, fVal ) );

		(m_owner->GetInterface())->I_DPrintf( WL_e::WL_DEBUG, "%4d camera( PAN, <%f %f %f>, <%f %f %f>, %f); [%d]", m_ownerID, vector[0], vector[1], vector[2], vector2[0], vector2[1], vector2[2], fVal, task->GetTimeStamp() );
		ie->I_CameraPan( vector, vector2, fVal );
		break;

	case icarusToken_t::TYPE_ZOOM:

		ICARUS_VALIDATE( GetFloat( m_ownerID, block, memberNum, fVal ) );
		ICARUS_VALIDATE( GetFloat( m_ownerID, block, memberNum, fVal2 ) );

		(m_owner->GetInterface())->I_DPrintf( WL_e::WL_DEBUG, "%4d camera( ZOOM, %f, %f); [%d]", m_ownerID, fVal, fVal2, task->GetTimeStamp() );
		ie->I_CameraZoom( fVal, fVal2 );
		break;

	case icarusToken_t::TYPE_MOVE:

		ICARUS_VALIDATE( GetVector( m_ownerID, block, memberNum, vector ) );
		ICARUS_VALIDATE( GetFloat( m_ownerID, block, memberNum, fVal ) );

		(m_owner->GetInterface())->I_DPrintf( WL_e::WL_DEBUG, "%4d camera( MOVE, <%f %f %f>, %f); [%d]", m_ownerID, vector[0], vector[1], vector[2], fVal, task->GetTimeStamp() );
		ie->I_CameraMove( vector, fVal );
		break;

	case icarusToken_t::TYPE_ROLL:

		ICARUS_VALIDATE( GetFloat( m_ownerID, block, memberNum, fVal ) );
		ICARUS_VALIDATE( GetFloat( m_ownerID, block, memberNum, fVal2 ) );

		(m_owner->GetInterface())->I_DPrintf( WL_e::WL_DEBUG, "%4d camera( ROLL, %f, %f); [%d]", m_ownerID, fVal, fVal2, task->GetTimeStamp() );
		ie->I_CameraRoll( fVal, fVal2 );

		break;

	case icarusToken_t::TYPE_FOLLOW:

		ICARUS_VALIDATE( Get( m_ownerID, block, memberNum, &sVal ) );
		ICARUS_VALIDATE( GetFloat( m_ownerID, block, memberNum, fVal ) );
		ICARUS_VALIDATE( GetFloat( m_ownerID, block, memberNum, fVal2 ) );

		(m_owner->GetInterface())->I_DPrintf( WL_e::WL_DEBUG, "%4d camera( FOLLOW, \"%s\", %f, %f); [%d]", m_ownerID, sVal, fVal, fVal2, task->GetTimeStamp() );
		ie->I_CameraFollow( (const char *) sVal, fVal, fVal2 );

		break;

	case icarusToken_t::TYPE_TRACK:

		ICARUS_VALIDATE( Get( m_ownerID, block, memberNum, &sVal ) );
		ICARUS_VALIDATE( GetFloat( m_ownerID, block, memberNum, fVal ) );
		ICARUS_VALIDATE( GetFloat( m_ownerID, block, memberNum, fVal2 ) );

		(m_owner->GetInterface())->I_DPrintf( WL_e::WL_DEBUG, "%4d camera( TRACK, \"%s\", %f, %f); [%d]", m_ownerID, sVal, fVal, fVal2, task->GetTimeStamp() );
		ie->I_CameraTrack( (const char *) sVal, fVal, fVal2 );
		break;

	case icarusToken_t::TYPE_DISTANCE:

		ICARUS_VALIDATE( GetFloat( m_ownerID, block, memberNum, fVal ) );
		ICARUS_VALIDATE( GetFloat( m_ownerID, block, memberNum, fVal2 ) );

		(m_owner->GetInterface())->I_DPrintf( WL_e::WL_DEBUG, "%4d camera( DISTANCE, %f, %f); [%d]", m_ownerID, fVal, fVal2, task->GetTimeStamp() );
		ie->I_CameraDistance( fVal, fVal2 );
		break;

	case icarusToken_t::TYPE_FADE:

		ICARUS_VALIDATE( GetVector( m_ownerID, block, memberNum, vector ) );
		ICARUS_VALIDATE( GetFloat( m_ownerID, block, memberNum, fVal ) );

		ICARUS_VALIDATE( GetVector( m_ownerID, block, memberNum, vector2 ) );
		ICARUS_VALIDATE( GetFloat( m_ownerID, block, memberNum, fVal2 ) );

		ICARUS_VALIDATE( GetFloat( m_ownerID, block, memberNum, fVal3 ) );

		(m_owner->GetInterface())->I_DPrintf( WL_e::WL_DEBUG, "%4d camera( FADE, <%f %f %f>, %f, <%f %f %f>, %f, %f); [%d]", m_ownerID, vector[0], vector[1], vector[2], fVal, vector2[0], vector2[1], vector2[2], fVal2, fVal3, task->GetTimeStamp() );
		ie->I_CameraFade( vector[0], vector[1], vector[2], fVal, vector2[0], vector2[1], vector2[2], fVal2, fVal3 );
		break;

	case icarusToken_t::TYPE_PATH:
		ICARUS_VALIDATE( Get( m_ownerID, block, memberNum, &sVal ) );

		(m_owner->GetInterface())->I_DPrintf( WL_e::WL_DEBUG, "%4d camera( PATH, \"%s\"); [%d]", m_ownerID, sVal, task->GetTimeStamp() );
		ie->I_CameraPath( sVal );
		break;

	case icarusToken_t::TYPE_ENABLE:
		(m_owner->GetInterface())->I_DPrintf( WL_e::WL_DEBUG, "%4d camera( ENABLE ); [%d]", m_ownerID, task->GetTimeStamp() );
		ie->I_CameraEnable();
		break;

	case icarusToken_t::TYPE_DISABLE:
		(m_owner->GetInterface())->I_DPrintf( WL_e::WL_DEBUG, "%4d camera( DISABLE ); [%d]", m_ownerID, task->GetTimeStamp() );
		ie->I_CameraDisable();
		break;

	case icarusToken_t::TYPE_SHAKE:
		ICARUS_VALIDATE( GetFloat( m_ownerID, block, memberNum, fVal ) );
		ICARUS_VALIDATE( GetFloat( m_ownerID, block, memberNum, fVal2 ) );

		(m_owner->GetInterface())->I_DPrintf( WL_e::WL_DEBUG, "%4d camera( SHAKE, %f, %f ); [%d]", m_ownerID, fVal, fVal2, task->GetTimeStamp() );
		ie->I_CameraShake( fVal, (int) fVal2 );
		break;

	default:
		break;
	}

	Completed( task->GetGUID() );

	return taskResult_t::TASK_OK;
}

/*
-------------------------
Move
-------------------------
*/

taskResult_t CTaskManager::Move( CTask *task )
{
	vector_t	vector, vector2;
	CBlock		*block = task->GetBlock();
	float		duration;
	int			memberNum = 0;

	//Get the goal position
	ICARUS_VALIDATE( GetVector( m_ownerID, block, memberNum, vector ) );

	//Check for possible angles field
	if ( GetVector( m_ownerID, block, memberNum, vector2 ) == false )
	{
		ICARUS_VALIDATE( GetFloat( m_ownerID, block, memberNum, duration ) );


		(m_owner->GetInterface())->I_DPrintf( WL_e::WL_DEBUG, "%4d move( <%f %f %f>, %f ); [%d]", m_ownerID, vector[0], vector[1], vector[2], duration, task->GetTimeStamp() );
		(m_owner->GetInterface())->I_Lerp2Pos( task->GetGUID(), m_ownerID, vector, nullptr, duration );

		return taskResult_t::TASK_OK;
	}

	//Get the duration and make the call
	ICARUS_VALIDATE( GetFloat( m_ownerID, block, memberNum, duration ) );

	(m_owner->GetInterface())->I_DPrintf( WL_e::WL_DEBUG, "%4d move( <%f %f %f>, <%f %f %f>, %f ); [%d]", m_ownerID, vector[0], vector[1], vector[2], vector2[0], vector2[1], vector2[2], duration, task->GetTimeStamp() );
	(m_owner->GetInterface())->I_Lerp2Pos( task->GetGUID(), m_ownerID, vector, vector2, duration );

	return taskResult_t::TASK_OK;
}

/*
-------------------------
Kill
-------------------------
*/

taskResult_t CTaskManager::Kill( CTask *task )
{
	CBlock	*block = task->GetBlock();
	const char	*sVal;
	int		memberNum = 0;

	ICARUS_VALIDATE( Get( m_ownerID, block, memberNum, &sVal ) );

	(m_owner->GetInterface())->I_DPrintf( WL_e::WL_DEBUG, "%4d kill( \"%s\" ); [%d]", m_ownerID, sVal, task->GetTimeStamp() );
	(m_owner->GetInterface())->I_Kill( m_ownerID, sVal );

	Completed( task->GetGUID() );

	return taskResult_t::TASK_OK;
}

/*
-------------------------
Set
-------------------------
*/

taskResult_t CTaskManager::Set( CTask *task )
{
	CBlock			*block = task->GetBlock();
	const char			*sVal, *sVal2;
	int				memberNum = 0;

	ICARUS_VALIDATE( Get( m_ownerID, block, memberNum, &sVal ) );
	ICARUS_VALIDATE( Get( m_ownerID, block, memberNum, &sVal2 ) );

	(m_owner->GetInterface())->I_DPrintf( WL_e::WL_DEBUG, "%4d set( \"%s\", \"%s\" ); [%d]", m_ownerID, sVal, sVal2, task->GetTimeStamp() );
	(m_owner->GetInterface())->I_Set( task->GetGUID(), m_ownerID, sVal, sVal2 );

	return taskResult_t::TASK_OK;
}

/*
-------------------------
Use
-------------------------
*/

taskResult_t CTaskManager::Use( CTask *task )
{
	CBlock	*block = task->GetBlock();
	const char	*sVal;
	int		memberNum = 0;

	ICARUS_VALIDATE( Get( m_ownerID, block, memberNum, &sVal ) );

	(m_owner->GetInterface())->I_DPrintf( WL_e::WL_DEBUG, "%4d use( \"%s\" ); [%d]", m_ownerID, sVal, task->GetTimeStamp() );
	(m_owner->GetInterface())->I_Use( m_ownerID, sVal );

	Completed( task->GetGUID() );

	return taskResult_t::TASK_OK;
}

/*
-------------------------
DeclareVariable
-------------------------
*/

taskResult_t CTaskManager::DeclareVariable( CTask *task )
{
	CBlock	*block = task->GetBlock();
	const char	*sVal;
	int		memberNum = 0;
	icarusToken_t	type;

	ICARUS_VALIDATE( GetToken( m_ownerID, block, memberNum, type ) );
	ICARUS_VALIDATE( Get( m_ownerID, block, memberNum, &sVal ) );

	(m_owner->GetInterface())->I_DPrintf( WL_e::WL_DEBUG, "%4d declare( %d, \"%s\" ); [%d]", m_ownerID, to_underlying( type ), sVal, task->GetTimeStamp() );
	(m_owner->GetInterface())->I_DeclareVariable( type, sVal );

	Completed( task->GetGUID() );

	return taskResult_t::TASK_OK;

}

/*
-------------------------
FreeVariable
-------------------------
*/

taskResult_t CTaskManager::FreeVariable( CTask *task )
{
	CBlock	*block = task->GetBlock();
	const char	*sVal;
	int		memberNum = 0;

	ICARUS_VALIDATE( Get( m_ownerID, block, memberNum, &sVal ) );

	(m_owner->GetInterface())->I_DPrintf( WL_e::WL_DEBUG, "%4d free( \"%s\" ); [%d]", m_ownerID, sVal, task->GetTimeStamp() );
	(m_owner->GetInterface())->I_FreeVariable( sVal );

	Completed( task->GetGUID() );

	return taskResult_t::TASK_OK;

}

/*
-------------------------
Signal
-------------------------
*/

taskResult_t CTaskManager::Signal( CTask *task )
{
	CBlock	*block = task->GetBlock();
	const char	*sVal;
	int		memberNum = 0;

	ICARUS_VALIDATE( Get( m_ownerID, block, memberNum, &sVal ) );

	(m_owner->GetInterface())->I_DPrintf( WL_e::WL_DEBUG, "%4d signal( \"%s\" ); [%d]", m_ownerID, sVal, task->GetTimeStamp() );
	m_owner->GetOwner()->Signal( (const char *) sVal );

	Completed( task->GetGUID() );

	return taskResult_t::TASK_OK;
}

/*
-------------------------
Play
-------------------------
*/

taskResult_t CTaskManager::Play( CTask *task )
{
	CBlock	*block = task->GetBlock();
	const char	*sVal, *sVal2;
	int		memberNum = 0;

	ICARUS_VALIDATE( Get( m_ownerID, block, memberNum, &sVal ) );
	ICARUS_VALIDATE( Get( m_ownerID, block, memberNum, &sVal2 ) );

	(m_owner->GetInterface())->I_DPrintf( WL_e::WL_DEBUG, "%4d play( \"%s\", \"%s\" ); [%d]", m_ownerID, sVal, sVal2, task->GetTimeStamp() );
	(m_owner->GetInterface())->I_Play( task->GetGUID(), m_ownerID, (const char *) sVal, (const char *) sVal2 );

	return taskResult_t::TASK_OK;
}

/*
-------------------------
SaveCommand
-------------------------
*/

//FIXME: ARGH!  This is duplicated from CSequence because I can't directly link it any other way...

int CTaskManager::SaveCommand( CBlock *block )
{
	unsigned char	flags;
	int				numMembers, bID, size;
	CBlockMember	*bm;

	ojk::SavedGameHelper saved_game(
		m_owner->GetInterface()->saved_game);

	//Save out the block ID
	bID = to_underlying( block->GetBlockID() );

	saved_game.write_chunk<int32_t>(
		INT_ID('B', 'L', 'I', 'D'),
		bID);

	//Save out the block's flags
	flags = to_underlying( block->GetFlags() );

	saved_game.write_chunk<uint8_t>(
		INT_ID('B', 'F', 'L', 'G'),
		flags);

	//Save out the number of members to read
	numMembers = block->GetNumMembers();

	saved_game.write_chunk<int32_t>(
		INT_ID('B', 'N', 'U', 'M'),
		numMembers);

	for ( int i = 0; i < numMembers; i++ )
	{
		bm = block->GetMember( i );

		//Save the block id
		bID = to_underlying( bm->GetID() );

		saved_game.write_chunk<int32_t>(
			INT_ID('B', 'M', 'I', 'D'),
			bID);

		//Save out the data size
		size = bm->GetSize();

		saved_game.write_chunk<int32_t>(
			INT_ID('B', 'S', 'I', 'Z'),
			size);

		//Save out the raw data
		const uint8_t* raw_data = static_cast<const uint8_t*>(bm->GetData());

		saved_game.write_chunk(
			INT_ID('B', 'M', 'E', 'M'),
			raw_data,
			size);
	}

	return true;
}

/*
-------------------------
Save
-------------------------
*/

void CTaskManager::Save( void )
{
	CTaskGroup	*taskGroup;
	const char	*name;
	CBlock		*block;
	uint32_t	timeStamp;
	bool		completed;
	int			id, numCommands;
	[[maybe_unused]] int	numWritten;

	ojk::SavedGameHelper saved_game(
		m_owner->GetInterface()->saved_game);

	//Save the taskmanager's GUID
	saved_game.write_chunk<int32_t>(
		INT_ID('T', 'M', 'I', 'D'),
		m_GUID); //FIXME: This can be reconstructed

	//Save out the number of tasks that will follow
	int iNumTasks = m_tasks.size();

	saved_game.write_chunk<int32_t>(
		INT_ID('T', 'S', 'K', '#'),
		iNumTasks);

	//Save out all the tasks
	tasks_l::iterator	ti;

	STL_ITERATE( ti, m_tasks )
	{
		//Save the GUID
		id = (*ti)->GetGUID();

		saved_game.write_chunk<int32_t>(
			INT_ID('T', 'K', 'I', 'D'),
			id);

		//Save the timeStamp (FIXME: Although, this is going to be worthless if time is not consistent...)
		timeStamp = (*ti)->GetTimeStamp();

		saved_game.write_chunk<uint32_t>(
			INT_ID('T', 'K', 'T', 'S'),
			timeStamp);

		//Save out the block
		block = (*ti)->GetBlock();
		SaveCommand( block );
	}

	//Save out the number of task groups
	int numTaskGroups = m_taskGroups.size();

	saved_game.write_chunk<int32_t>(
		INT_ID('T', 'G', '#', 'G'),
		numTaskGroups);

	//Save out the IDs of all the task groups
	numWritten = 0;
	taskGroup_v::iterator	tgi;
	STL_ITERATE( tgi, m_taskGroups )
	{
		id = (*tgi)->GetGUID();

		saved_game.write_chunk<int32_t>(
			INT_ID('T', 'K', 'G', '#'),
			id);

		numWritten++;
	}
	assert (numWritten == numTaskGroups);

	//Save out the task groups
	numWritten = 0;
	STL_ITERATE( tgi, m_taskGroups )
	{
		//Save out the parent
		id = ( (*tgi)->GetParent() == nullptr ) ? -1 : ((*tgi)->GetParent())->GetGUID();

		saved_game.write_chunk<int32_t>(
			INT_ID('T', 'K', 'G', 'P'),
			id);

		//Save out the number of commands
		numCommands = (*tgi)->m_completedTasks.size();

		saved_game.write_chunk<int32_t>(
			INT_ID('T', 'G', 'N', 'C'),
			numCommands);

		//Save out the command map
		CTaskGroup::taskCallback_m::iterator	tci;

		STL_ITERATE( tci, (*tgi)->m_completedTasks )
		{
			//Write out the ID
			id = (*tci).first;

			saved_game.write_chunk<int32_t>(
				INT_ID('G', 'M', 'I', 'D'),
				id);

			//Write out the state of completion
			completed = (*tci).second;

			saved_game.write_chunk<uint8_t>(
				INT_ID('G', 'M', 'D', 'N'),
				completed);
		}

		//Save out the number of completed commands
		id = (*tgi)->m_numCompleted;

		saved_game.write_chunk<int32_t>(
			INT_ID('T', 'G', 'D', 'N'),
			id); //FIXME: This can be reconstructed

		numWritten++;
	}
	assert (numWritten == numTaskGroups);

	//Only bother if we've got tasks present
	if ( m_taskGroups.size() )
	{
		//Save out the currently active group
		int	curGroupID = ( m_curGroup == nullptr ) ? -1 : m_curGroup->GetGUID();

		saved_game.write_chunk<int32_t>(
			INT_ID('T', 'G', 'C', 'G'),
			curGroupID);
	}

	//Save out the task group name maps
	taskGroupName_m::iterator	tmi;
	numWritten = 0;
	STL_ITERATE( tmi, m_taskGroupNameMap )
	{
		name = ((*tmi).first).c_str();

		//Make sure this is a valid string
		assert( ( name != nullptr ) && ( name[0] != '\0' ) );

		int length = strlen( name ) + 1;

		//Save out the string size
		saved_game.write_chunk<int32_t>(
			INT_ID('T', 'G', 'N', 'L'),
			length);

		//Write out the string
		saved_game.write_chunk(
			INT_ID('T', 'G', 'N', 'S'),
			name,
			length);

		taskGroup = (*tmi).second;

		id = taskGroup->GetGUID();

		//Write out the ID
		saved_game.write_chunk<int32_t>(
			INT_ID('T', 'G', 'N', 'I'),
			id);

		numWritten++;
	}
	assert (numWritten == numTaskGroups);
}

/*
-------------------------
Load
-------------------------
*/

void CTaskManager::Load( void )
{
	unsigned char	flags = 0;
	CTaskGroup		*taskGroup;
	CBlock			*block;
	CTask			*task;
	uint32_t		timeStamp = 0;
	bool			completed = false;
	void			*bData;
	int				id = 0, numTasks = 0, numMembers = 0;
	int				bID = 0, bSize = 0;
	int				i;

	ojk::SavedGameHelper saved_game(
		m_owner->GetInterface()->saved_game);

	//Get the GUID
	saved_game.read_chunk<int32_t>(
		INT_ID('T', 'M', 'I', 'D'),
		m_GUID);

	//Get the number of tasks to follow
	saved_game.read_chunk<int32_t>(
		INT_ID('T', 'S', 'K', '#'),
		numTasks);

	//Reload all the tasks
	for ( i = 0; i < numTasks; i++ )
	{
		task = new CTask;

		assert( task );

		//Get the GUID
		saved_game.read_chunk<int32_t>(
			INT_ID('T', 'K', 'I', 'D'),
			id);

		task->SetGUID( id );

		//Get the time stamp
		saved_game.read_chunk<uint32_t>(
			INT_ID('T', 'K', 'T', 'S'),
			timeStamp);

		task->SetTimeStamp( timeStamp );

		//
		// BLOCK LOADING
		//

		//Get the block ID and create a new container
		saved_game.read_chunk<int32_t>(
			INT_ID('B', 'L', 'I', 'D'),
			id);

		block = new CBlock;

		block->Create( static_cast<icarusToken_t>( id ) );

		//Read the block's flags
		saved_game.read_chunk<uint8_t>(
			INT_ID('B', 'F', 'L', 'G'),
			flags);

		block->SetFlags( static_cast<blockFlags_t>( flags ) );

		//Get the number of block members
		saved_game.read_chunk<int32_t>(
			INT_ID('B', 'N', 'U', 'M'),
			numMembers);

		for ( int j = 0; j < numMembers; j++ )
		{
			//Get the member ID
			saved_game.read_chunk<int32_t>(
				INT_ID('B', 'M', 'I', 'D'),
				bID);

			//Get the member size
			saved_game.read_chunk<int32_t>(
				INT_ID('B', 'S', 'I', 'Z'),
				bSize);

			//Get the member's data
			if ( ( bData = ICARUS_Malloc( bSize ) ) == nullptr )
			{
				assert( 0 );
				return;
			}

			//Get the actual raw data
			saved_game.read_chunk(
				INT_ID('B', 'M', 'E', 'M'),
				static_cast<uint8_t*>(bData),
				bSize);

			//Write out the correct type
			const icarusToken_t memberID = static_cast<icarusToken_t>( bID );
			switch ( memberID )
			{
			case icarusToken_t::TK_FLOAT:
				block->Write( icarusToken_t::TK_FLOAT, *(float *) bData );
				break;

			case icarusToken_t::TK_IDENTIFIER:
				block->Write( icarusToken_t::TK_IDENTIFIER, (char *) bData );
				break;

			case icarusToken_t::TK_STRING:
				block->Write( icarusToken_t::TK_STRING, (char *) bData );
				break;

			case icarusToken_t::TK_VECTOR:
				block->Write( icarusToken_t::TK_VECTOR, *(vec3_t *) bData );
				break;

			case icarusToken_t::ID_RANDOM:
				block->Write( icarusToken_t::ID_RANDOM, *(float *) bData );//to_underlying( icarusToken_t::ID_RANDOM ) );
				break;

			case icarusToken_t::ID_TAG:
				block->Write( icarusToken_t::ID_TAG, (float) to_underlying( icarusToken_t::ID_TAG ) );
				break;

			case icarusToken_t::ID_GET:
				block->Write( icarusToken_t::ID_GET, (float) to_underlying( icarusToken_t::ID_GET ) );
				break;

			default:
				(m_owner->GetInterface())->I_DPrintf( WL_e::WL_ERROR, "Invalid Block id %d\n", bID);
				assert( 0 );
				break;
			}

			//Get rid of the temp memory
			ICARUS_Free( bData );
		}

		task->SetBlock( block );

		STL_INSERT( m_tasks, task );
	}

	//Load the task groups
	int numTaskGroups = 0;

	saved_game.read_chunk<int32_t>(
		INT_ID('T', 'G', '#', 'G'),
		numTaskGroups);

	if ( numTaskGroups == 0 )
		return;

	int *taskIDs = new int[ numTaskGroups ];

	//Get the task group IDs
	for ( i = 0; i < numTaskGroups; i++ )
	{
		//Creat a new task group
		taskGroup = new CTaskGroup;
		assert( taskGroup );

		//Get this task group's ID
		saved_game.read_chunk<int32_t>(
			INT_ID('T', 'K', 'G', '#'),
			taskIDs[i]);

		taskGroup->m_GUID = taskIDs[i];

		m_taskGroupIDMap[ taskIDs[i] ] = taskGroup;

		STL_INSERT( m_taskGroups, taskGroup );
	}

	//Recreate and load the task groups
	for ( i = 0; i < numTaskGroups; i++ )
	{
		taskGroup = GetTaskGroup( taskIDs[i] );
		assert( taskGroup );

		//Load the parent ID
		saved_game.read_chunk<int32_t>(
			INT_ID('T', 'K', 'G', 'P'),
			id);

		if ( id != -1 )
			taskGroup->m_parent = ( GetTaskGroup( id ) != nullptr ) ? GetTaskGroup( id ) : nullptr;

		//Get the number of commands in this group
		saved_game.read_chunk<int32_t>(
			INT_ID('T', 'G', 'N', 'C'),
			numMembers);

		//Get each command and its completion state
		for ( int j = 0; j < numMembers; j++ )
		{
			//Get the ID
			saved_game.read_chunk<int32_t>(
				INT_ID('G', 'M', 'I', 'D'),
				id);

			//Write out the state of completion
			saved_game.read_chunk<uint8_t>(
				INT_ID('G', 'M', 'D', 'N'),
				completed);

			//Save it out
			taskGroup->m_completedTasks[ id ] = completed;
		}

		//Get the number of completed tasks
		saved_game.read_chunk<int32_t>(
			INT_ID('T', 'G', 'D', 'N'),
			taskGroup->m_numCompleted);
	}

	//Reload the currently active group
	int curGroupID;

	saved_game.read_chunk<int32_t>(
		INT_ID('T', 'G', 'C', 'G'),
		curGroupID);

	//Reload the map entries
	for ( i = 0; i < numTaskGroups; i++ )
	{
		char	name[1024];
		int		length = 0;

		//Get the size of the string
		saved_game.read_chunk<int32_t>(
			INT_ID('T', 'G', 'N', 'L'),
			length);

		//Get the string
		saved_game.read_chunk(
			INT_ID('T', 'G', 'N', 'S'),
			name,
			length);

		//Get the id
		saved_game.read_chunk<int32_t>(
			INT_ID('T', 'G', 'N', 'I'),
			id);

		taskGroup = GetTaskGroup( id );
		assert( taskGroup );

		m_taskGroupNameMap[ name ] = taskGroup;
		m_taskGroupIDMap[ taskGroup->GetGUID() ] = taskGroup;
	}

	m_curGroup = ( curGroupID == -1 ) ? nullptr : m_taskGroupIDMap[curGroupID];

	delete[] taskIDs;
}
