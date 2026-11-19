/*
===========================================================================
Copyright (C) 1999 - 2005, Id Software, Inc.
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

//NOTE:	int =	default signed
//				default long

inline constexpr int AASID = ('S'<<24)+('A'<<16)+('A'<<8)+'E';
inline constexpr int AASVERSION_OLD = 4;
inline constexpr int AASVERSION = 5;

//presence types
enum class presenceFlags_t
{
	PRESENCE_EMPTY = 0,			//no presence type (PRESENCE_NONE is a type: not in any area)
	PRESENCE_NONE				= 1,
	PRESENCE_NORMAL				= 2,
	PRESENCE_CROUCH				= 4,
};
template <> inline constexpr bool enableEnumFlags<presenceFlags_t> = true;

//travel types
inline constexpr int MAX_TRAVELTYPES = 32;
enum class travelTypeFlags_t
{
	TRAVEL_NONE = 0,
	TRAVEL_INVALID				= 1,		//temporary not possible
	TRAVEL_WALK					= 2,		//walking
	TRAVEL_CROUCH				= 3,		//crouching
	TRAVEL_BARRIERJUMP			= 4,		//jumping onto a barrier
	TRAVEL_JUMP					= 5,		//jumping
	TRAVEL_LADDER				= 6,		//climbing a ladder
	TRAVEL_WALKOFFLEDGE			= 7,		//walking of a ledge
	TRAVEL_SWIM					= 8,		//swimming
	TRAVEL_WATERJUMP			= 9,		//jump out of the water
	TRAVEL_TELEPORT				= 10,		//teleportation
	TRAVEL_ELEVATOR				= 11,		//travel by elevator
	TRAVEL_ROCKETJUMP			= 12,		//rocket jumping required for travel
	TRAVEL_BFGJUMP				= 13,		//bfg jumping required for travel
	TRAVEL_GRAPPLEHOOK			= 14,		//grappling hook required for travel
	TRAVEL_DOUBLEJUMP			= 15,		//double jump
	TRAVEL_RAMPJUMP				= 16,		//ramp jump
	TRAVEL_STRAFEJUMP			= 17,		//strafe jump
	TRAVEL_JUMPPAD				= 18,		//jump pad
	TRAVEL_FUNCBOB				= 19,		//func bob

//additional travel flags
	TRAVELTYPE_MASK				= 0xFFFFFF,
	TRAVELFLAG_NOTTEAM1			= (1 << 24),
	TRAVELFLAG_NOTTEAM2			= (2 << 24),
};
template <> inline constexpr bool enableEnumFlags<travelTypeFlags_t> = true;

//face flags
enum class faceFlags_t
{
	FACE_NONE = 0,
	FACE_SOLID					= 1,		//just solid at the other side
	FACE_LADDER					= 2,		//ladder
	FACE_GROUND					= 4,		//standing on ground when in this face
	FACE_GAP					= 8,		//gap in the ground
	FACE_LIQUID					= 16,		//face separating two areas with liquid
	FACE_LIQUIDSURFACE			= 32,		//face separating liquid and air
	FACE_BRIDGE					= 64,		//can walk over this face if bridge is closed
};
template <> inline constexpr bool enableEnumFlags<faceFlags_t> = true;

//area contents
inline constexpr int AREACONTENTS_MODELNUMSHIFT = 24;
inline constexpr int AREACONTENTS_MAXMODELNUM = 0xFF;
enum class areaContentsFlags_t : uint32_t
{
	AREACONTENTS_NONE = 0,
	AREACONTENTS_WATER				= 1,
	AREACONTENTS_LAVA				= 2,
	AREACONTENTS_SLIME				= 4,
	AREACONTENTS_CLUSTERPORTAL		= 8,
	AREACONTENTS_TELEPORTAL			= 16,
	AREACONTENTS_ROUTEPORTAL		= 32,
	AREACONTENTS_TELEPORTER			= 64,
	AREACONTENTS_JUMPPAD			= 128,
	AREACONTENTS_DONOTENTER			= 256,
	AREACONTENTS_VIEWPORTAL			= 512,
	AREACONTENTS_MOVER				= 1024,
	AREACONTENTS_NOTTEAM1			= 2048,
	AREACONTENTS_NOTTEAM2			= 4096,
//number of model of the mover inside this area
	AREACONTENTS_MODELNUM			= (uint32_t{ AREACONTENTS_MAXMODELNUM } << AREACONTENTS_MODELNUMSHIFT),
};
template <> inline constexpr bool enableEnumFlags<areaContentsFlags_t> = true;

//area flags
enum class areaFlags_t
{
	AREA_NONE = 0,
	AREA_GROUNDED				= 1,		//bot can stand on the ground
	AREA_LADDER					= 2,		//area contains one or more ladder faces
	AREA_LIQUID					= 4,		//area contains a liquid
	AREA_DISABLED				= 8,		//area is disabled for routing when set
	AREA_BRIDGE					= 16,		//area ontop of a bridge
//area flag used for weapon jumping (be_aas_reach.cpp)
	AREA_WEAPONJUMP				= 8192,	//valid area to weapon jump to
};
template <> inline constexpr bool enableEnumFlags<areaFlags_t> = true;

//aas file header lumps
enum class aasLump_t
{
	AASLUMP_BBOXES				= 0,
	AASLUMP_VERTEXES			= 1,
	AASLUMP_PLANES				= 2,
	AASLUMP_EDGES				= 3,
	AASLUMP_EDGEINDEX			= 4,
	AASLUMP_FACES				= 5,
	AASLUMP_FACEINDEX			= 6,
	AASLUMP_AREAS				= 7,
	AASLUMP_AREASETTINGS		= 8,
	AASLUMP_REACHABILITY		= 9,
	AASLUMP_NODES				= 10,
	AASLUMP_PORTALS				= 11,
	AASLUMP_PORTALINDEX			= 12,
	AASLUMP_CLUSTERS			= 13,
	AAS_LUMPS					= 14
};

//========== bounding box =========

//bounding box
typedef struct aas_bbox_s
{
	presenceFlags_t presencetype;
	int flags;
	vec3_t mins, maxs;
} aas_bbox_t;

//============ settings ===========

//reachability to another area
typedef struct aas_reachability_s
{
	int areanum;						//number of the reachable area
	int facenum;						//number of the face towards the other area
	int edgenum;						//number of the edge towards the other area
	vec3_t start;						//start point of inter area movement
	vec3_t end;							//end point of inter area movement
	travelTypeFlags_t traveltype;					//type of travel required to get to the area
	unsigned short int traveltime;//travel time of the inter area movement
} aas_reachability_t;

//area settings
typedef struct aas_areasettings_s
{
	//could also add all kind of statistic fields
	areaContentsFlags_t contents;						//contents of the area
	areaFlags_t areaflags;						//several area flags
	presenceFlags_t presencetype;					//how a bot can be present in this area
	int cluster;						//cluster the area belongs to, if negative it's a portal
	int clusterareanum;				//number of the area in the cluster
	int numreachableareas;			//number of reachable areas from this one
	int firstreachablearea;			//first reachable area in the reachable area index
} aas_areasettings_t;

//cluster portal
typedef struct aas_portal_s
{
	int areanum;						//area that is the actual portal
	int frontcluster;					//cluster at front of portal
	int backcluster;					//cluster at back of portal
	int clusterareanum[2];			//number of the area in the front and back cluster
} aas_portal_t;

//cluster portal index
typedef int aas_portalindex_t;

//cluster
typedef struct aas_cluster_s
{
	int numareas;						//number of areas in the cluster
	int numreachabilityareas;			//number of areas with reachabilities
	int numportals;						//number of cluster portals
	int firstportal;					//first cluster portal in the index
} aas_cluster_t;

//============ 3d definition ============

typedef vec3_t aas_vertex_t;

//just a plane in the third dimension
typedef struct aas_plane_s
{
	vec3_t normal;						//normal vector of the plane
	float dist;							//distance of the plane (normal vector * distance = point in plane)
	int type;
} aas_plane_t;

//edge
typedef struct aas_edge_s
{
	int v[2];							//numbers of the vertexes of this edge
} aas_edge_t;

//edge index, negative if vertexes are reversed
typedef int aas_edgeindex_t;

//a face bounds an area, often it will also seperate two areas
typedef struct aas_face_s
{
	int planenum;						//number of the plane this face is in
	faceFlags_t faceflags;						//face flags (no use to create face settings for just this field)
	int numedges;						//number of edges in the boundary of the face
	int firstedge;						//first edge in the edge index
	int frontarea;						//area at the front of this face
	int backarea;						//area at the back of this face
} aas_face_t;

//face index, stores a negative index if backside of face
typedef int aas_faceindex_t;

//area with a boundary of faces
typedef struct aas_area_s
{
	int areanum;						//number of this area
	//3d definition
	int numfaces;						//number of faces used for the boundary of the area
	int firstface;						//first face in the face index used for the boundary of the area
	vec3_t mins;						//mins of the area
	vec3_t maxs;						//maxs of the area
	vec3_t center;						//'center' of the area
} aas_area_t;

//nodes of the bsp tree
typedef struct aas_node_s
{
	int planenum;
	int children[2];					//child nodes of this node, or areas as leaves when negative
										//when a child is zero it's a solid leaf
} aas_node_t;

//=========== aas file ===============

//header lump
typedef struct aas_lump_s {
	int fileofs;
	int filelen;
} aas_lump_t;

//aas file header
typedef struct aas_header_s
{
	int ident;
	int version;
	int bspchecksum;
	//data entries
	EnumArray<aas_lump_t, aasLump_t, aasLump_t::AAS_LUMPS> lumps;
} aas_header_t;


//====== additional information ======
/*

-	when a node child is a solid leaf the node child number is zero
-	two adjacent areas (sharing a plane at opposite sides) share a face
	this face is a portal between the areas
-	when an area uses a face from the faceindex with a positive index
	then the face plane normal points into the area
-	the face edges are stored counter clockwise using the edgeindex
-	two adjacent convex areas (sharing a face) only share One face
	this is a simple result of the areas being convex
-	the areas can't have a mixture of ground and gap faces
	other mixtures of faces in one area are allowed
-	areas with the AREACONTENTS_CLUSTERPORTAL in the settings have
	the cluster number set to the negative portal number
-	edge zero is a dummy
-	face zero is a dummy
-	area zero is a dummy
-	node zero is a dummy
*/
