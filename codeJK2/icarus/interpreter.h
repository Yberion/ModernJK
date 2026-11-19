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

// Interpreter.h

#ifndef __INTERPRETER__
#define __INTERPRETER__

#include "tokens.h"

#include "qcommon/q_icarusflags.h"


typedef float	vector_t[3];

typedef struct variable_s
{
	char	name[MAX_VAR_NAME];
	icarusToken_t	type;
	void	*data;
} variable_t;

typedef std::map< std::string, variable_t * >	variable_m;
typedef std::vector < variable_t * > variable_v;

//CInterpreter

class CInterpreter
{
public:

	CInterpreter();
	~CInterpreter();

	int Interpret( CTokenizer *, CBlockStream *, char *filename=nullptr );	//Main interpretation function

	int Match( icarusToken_t );		//Looks ahead to the next token to try and match it to the passed token, consumes token on success
	int LookAhead( icarusToken_t );	//Looks ahead without consuming on success

	icarusToken_t FindSymbol( const char *,  keywordArray_t * );		//Searches the symbol table for the given name.  Returns the ID if found

	int GetAffect( void );		//Handles the affect() function
	int GetWait( void );		//Handles the wait() function
	int GetSet( void );			//Handles the set() function
	int GetBroadcast( void );	//Handles the broadcast() function
	int GetLoop( void );		//Handles the loop() function
	int GetPrint( void );		//Handles the print() function
	int GetUse( void );			//Handles the use() function
	int GetFlush( void );		//Handles the flush() function
	int	GetRun( void );			//Handles the run() function
	int	GetKill( void );		//Handles the kill() function
	int	GetRemove( void );		//Handles the remove() function
	int GetCamera( void );		//Handles the camera() function
	int GetIf( void );			//Handles the if() conditional statement
	int GetSound( void );		//Handles the sound() function
	int GetMove( void );		//Handles the move() function
	int GetRotate( void );		//Handles the rotate() function
	int GetRem( void );			//Handles the rem() function
	int	GetTask( void );
	int GetDo( void );
	int GetElse( void );
	int GetDeclare( void );
	int GetFree( void );
	int GetDoWait( void );
	int GetSignal( void );
	int GetWaitSignal( void );
	int GetPlay( void );

	int GetRandom( CBlock *block );
	int GetGet( CBlock *block );		//Heh
	int	GetTag( CBlock *block );		//Handles the tag() identifier
	int GetVector( CBlock *block );

	icarusToken_t GetNextType( void );

	int GetType( char *get );

	int GetAny( CBlock *block );
	int GetEvaluator( CBlock *block );
	int GetString( CBlock *);			//Attempts to match and retrieve the value of a string token
	int	GetIdentifier( CBlock *get );	//Attempts to match and retrieve the value of an identifier token
	int	GetInteger( CBlock * );		//Attempts to match and retrieve the value of a int token
	int GetFloat( CBlock * );		//Attempts to match and retrieve the value of a float token
	int GetVariable( icarusToken_t type );

	int GetID ( char * );	//Attempts to match and interpret an identifier

	keywordArray_t *GetSymbols( void )	{	return (keywordArray_t *) &m_symbolKeywords;	}	//Returns the interpreter's symbol table
	keywordArray_t *GetIDs( void )		{	return (keywordArray_t *) &m_IDKeywords;		}	//Returns the interpreter's ID table
	keywordArray_t *GetTypes( void )	{	return (keywordArray_t *) &m_typeKeywords;	}		//Returns the interpreter's type table

protected:

	void InitVars( void );
	void FreeVars( void );

	variable_t *AddVar( const char *name, icarusToken_t type );
	variable_t *FindVar( const char *name );

	const char *GetTokenName( icarusToken_t );	//Returns the name of a token
	int Error( char *, ... );			//Prints an error message

	int MatchTag( void );				//Attempts to match to a tag identifier
	int MatchGet( void );				//Attempts to match to a get identifier
	int	MatchRandom( void );			//Attempts to match to a random identifier

	CTokenizer	*m_tokenizer;			//Pointer to the tokenizer
	CBlockStream *m_blockStream;		//Pointer to the block stream

	variable_v	m_vars;
	variable_m	m_varMap;

	std::string	m_sCurrentLine;				// used in IBIze error reporting for more clarity
	std::string	m_sCurrentFile;				// full-pathed name of .TXT file (needed because of above, which affects parsestreams)
	int		m_iCurrentLine;				// also needed now because of 'm_sCurrentLine'
	int		m_iBadCBlockNumber;			// used for final app return code (NZ = err)

	static keywordArray_t	m_symbolKeywords[];		//Symbols
	static keywordArray_t	m_IDKeywords[];			//Identifiers
	static keywordArray_t	m_typeKeywords[];			//Types
	static keywordArray_t	m_conditionalKeywords[];	//Conditional
};

#endif	//__INTERPRETER__