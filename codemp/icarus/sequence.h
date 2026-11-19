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

#pragma once

// Sequence Header File

#include "blockstream.h"
#include "interface.h"
#include "taskmanager.h"

class ICARUS_Instance;

// The flags of a sequence
enum class sequenceFlags_t
{
	SQ_COMMON		= 0x00000000, 	//Common one-pass sequence
	SQ_LOOP			= 0x00000001, 	//Looping sequence
	SQ_RETAIN		= 0x00000002, 	//Inside a looping sequence list, retain the information
	SQ_AFFECT		= 0x00000004, 	//Affect sequence
	SQ_RUN			= 0x00000008,	//A run block
	SQ_PENDING		= 0x00000010,	//Pending use, don't free when flushing the sequences
	SQ_CONDITIONAL	= 0x00000020,	//Conditional statement
	SQ_TASK			= 0x00000040,	//Task block
};
template <> inline constexpr bool enableEnumFlags<sequenceFlags_t> = true;

class CSequence
{

	typedef std::list < CSequence * >	sequence_l;
	typedef	std::map	< int, CSequence *> sequenceID_m;
	typedef std::list < CBlock * >		block_l;

public:

	//Constructors / Destructors
	CSequence( void );
	~CSequence( void );

	//Creation and deletion
	static CSequence *Create( void );
	void Delete( void );

	//Organization functions
	void AddChild( CSequence * );
	void RemoveChild( CSequence * );

	void SetParent( CSequence * );
	CSequence *GetParent( void )	const	{	return m_parent;	}

	//Block manipulation
	CBlock *PopCommand( pushPop_t );
	int PushCommand( CBlock *, pushPop_t );

	//Flag utilties
	void SetFlag( sequenceFlags_t );
	void RemoveFlag( sequenceFlags_t, bool = false );
	bool HasFlag( sequenceFlags_t );
	sequenceFlags_t GetFlags( void )		const	{	return m_flags;		}
	void SetFlags( sequenceFlags_t flags )	{	m_flags = flags;	}

	//Various encapsulation utilities
	int GetIterations( void )		const	{	return m_iterations;	}
	void SetIterations( int it )	{	m_iterations = it;		}

	int GetID( void )		const	{	return m_id;			}
	void SetID( int id )	{	m_id = id;				}

	CSequence *GetReturn( void )	const	{	return m_return;		}

	void SetReturn ( CSequence *sequence );

	int GetNumCommands( void )	const	{	return m_numCommands;	}
	int GetNumChildren( void )	const	{	return (int)m_children.size();	}

	CSequence *GetChildByIndex( int id );
	bool HasChild( CSequence *sequence );

	void SetOwner( ICARUS_Instance *owner )	{	m_owner = owner;	}

	int Save( void );
	int Load( void );

	inline void *operator new( size_t size )
	{	// Allocate the memory.
		return Z_Malloc( size, memtag::TAG_ICARUS3, true );
	}
	// Overloaded delete operator.
	inline void operator delete( void *pRawData )
	{	// Free the Memory.
		Z_Free( pRawData );
	}

protected:

	int SaveCommand( CBlock *block );

	ICARUS_Instance			*m_owner;

	//Organization information
	sequence_l				m_children;
	CSequence				*m_parent;
	CSequence				*m_return;

	//Data information
	block_l					m_commands;
	sequenceFlags_t			m_flags;
	int						m_iterations;
	int						m_id;
	int						m_numCommands;
};
