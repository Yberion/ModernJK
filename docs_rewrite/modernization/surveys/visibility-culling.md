# Not sending the players a client cannot see: what recent games do, what JKA shows, the measures and the options against wallhacks

Written on 15 November 2026, for plan step 14 part 11, and set aside the same day after the prototype: plan step 16 ([../PLAN.md](../PLAN.md); [../progress/step-14-fixed-tick.md](../progress/step-14-fixed-tick.md) section 5). A wallhack draws the players the client was sent, wherever they are; the only defence that does not rely on the client is the server not sending a player the client cannot see. It is wanted, on one condition: it must be exact, with no loss of sight and no player shown late; a player may be revealed a little early, so that a wallhack shows only the very edge of a wall; the camera must be taken into account. Update rates by distance and priorities per client were set aside the same day (section 7). The prototype of option B, measured the same day, is in section 8; the design it leads to, still to decide, in section 9.

Each source is labelled as in the other surveys:

- **primary**: the studio's own article, talk slides or source code, or the engine's or library's own documentation;
- **secondary**: a summary written by someone else;
- **community**: forum posts, player guides, pull requests of forks.

## 1. Summary

1. Today the server sends a client every entity in the potentially visible set (PVS) of its eyes, the map's coarse cluster visibility: a player behind a wall in a visible cluster is sent, so a wallhack sees him.
2. Recent games test the line of sight on the server and send a player shortly before he can become visible: VALORANT from a precomputed cell-to-cell table (less than 2 % of its server frame), Counter-Strike with an occlusion test, Enemy Territory: Legacy (the same engine family) with point traces to the corners of a box. They all reveal early to cover the latency and accept that a player audible or about to appear is known to a cheat.
3. In JKA the camera is behind the player (third person is forced with the saber), its distance and height are the player's own settings, Force Sense level 2 draws players through walls, sounds come from the players' entities, and a saber blade reaches past a corner before its owner.
4. Measured here with 32 players on `mp/ffa3`: 91 bytes per snapshot for a client, 4.1 to 4.8 % of a core per tick for the server, 27 % of the pairs of players in each other's PVS, a point trace 1.9 to 3.3 microseconds under that load.
5. Tracing rays every tick for every pair would cost several times the server's present tick; a test reused while the players stay within a margin, or a table computed per map, is needed. The options are in section 6; the choices of the same day in section 7 (a table per map, prototyped first; the audible players kept; no limit for spectators and demos).
6. The prototype (section 8): a table built from the map alone, on the places a player can stand and the positions his camera can take, traced on the drawn triangles with tinybvh, misses 0.01 to 0.1 % of the visible pairs on the base maps (`mp/ffa2`, `mp/ffa3`, `mp/ctf5`), none seen from the eye. Combined with today's PVS it halves the hidden players a wallhack shows on `mp/ffa3` and divides them by seven on `mp/ctf5`, for 3 to 15 visible pairs in 55,800 not sent. It takes 5 to 16 s on one core of the development machine per base map and 95 s on `mp/siege_desert`, the largest, where it culls little at long range. Proposed after it (section 9): the table made beforehand by a tool and shipped with the map or placed by the administrator, never computed by a server.

## 2. What this engine sends today

`SV_BuildClientSnapshot` (`server/sv_snapshot.cpp`) takes the client's eyes (`ps.origin` plus `ps.viewheight`) and `SV_AddEntitiesVisibleFromPoint` adds every linked entity whose clusters are in the PVS of the eyes' cluster and whose area is connected to the eyes' area (`CM_AreasConnected`: a closed door's area portal cuts it), plus the entities sent to everyone (`SVF_BROADCAST`) and the portals' views. A distance cull exists (`g_svCullDist`), off. Nothing tests the line of sight: the PVS is computed per cluster when the map is compiled, and a cluster sees another if any point of one may see any point of the other.

## 3. What a JKA client can see

Read in cgame (`codemp/cgame`):

- **The camera.** Third person is forced with the saber, the melee fists, the emplaced guns and the E-Web, a grapple, a knockdown, a fall to death, a vehicle, and when dead; first person is forced while zoomed (`cg_view.c`, the choice of `cg.renderingThirdPerson`). The camera's target is the head raised by `cg_thirdPersonVertOffset` (16), traced from the head; the camera is `cg_thirdPersonRange` (80) behind the target along the view direction, turned by `cg_thirdPersonAngle` and `cg_thirdPersonPitchOffset`, traced from the target with a box of 8 units (`MASK_CAMERACLIP`), and it follows with a lag (`cg_thirdPersonCameraDamp`, `cg_thirdPersonTargetDamp`). So the camera always sees the head, within the range plus the height of it, but those four settings are the player's own (`CVAR_ARCHIVE`, no bound): a range of 500 looks round any corner. The server cannot know a camera it does not bound.
- **Force Sense.** From level 2, an active Force Sense draws the other players through walls (`RF_NODEPTH`, `cg_players.c`), with no range of its own: the PVS bounds it today. Level 1 only lights them.
- **Sounds.** Footsteps, the saber's hum and swings, shots and pains are the events and looping sounds of the players' entities (`cg_event.c`, `CG_S_AddLoopingSound`): a player not sent is not heard.
- **Sabers.** A blade is 32 units long by default (`bg_saberLoad.c`) and longer in some saber files; with the arm it reaches about 50 units from the body, past a corner before its owner, with its light on the walls.
- **What follows a player.** His missiles, his thrown saber and his effects are entities of their own, sent by their own visibility; a vehicle is larger than a player.
- **Prediction.** The client's movement collides with the other players of its snapshot; a player not sent cannot be touched, which only matters for a player against the viewer, never hidden.

## 4. What recent games do

| game | source | what it does |
|---|---|---|
| VALORANT | primary: Riot Games, "Demolishing Wallhacks with VALORANT's Fog of War" (2020) | The first prototype cast 10 rays from each networked actor to each player's point of view, "more than 350 times every frame" in a 10-player game, and doubled the frame time (128 tick back to 64). The shipped system uses a precomputed table: the map cut into cells, "as long as any part of a voxel can see any part of the other voxel, the table returns 'visible'", tested from the viewer's camera; the target's box is expanded by its velocity times a "look ahead" time "bigger than the expected ping time", so the enemy is sent "just before" he comes round the corner. No field of view is used. A hidden enemy is made "invisible and intangible" on the client, his position not sent; attached actors (his weapon) share his visibility; relevant actors are assumed to stay relevant briefly; effects are fast-forwarded when an actor comes back. "Less than 2% of server frame time (down from 50% in our first prototype)." What is audible was "tied in", not described; the system "drastically reduces the time enemy positions are available", it does not remove it |
| Counter-Strike: Global Offensive | community: AlliedModders forums | `sv_occlude_players` does not network occluded players, the same as the SourceMod anti-wallhack module, which costs "a great deal" of CPU with more players and made players "suddenly appear" for pings over 60 to 70 ms; an option runs the occlusion test on another thread |
| Counter-Strike 2 | secondary and community: articles, a server plugin | Server-side occlusion culling, its implementation not published; a community plugin stops sending a player hidden behind solid geometry |
| Enemy Territory: Legacy (Quake 3 engine family) | primary: its source, `src/server/sv_wallhack.c`, after Laszlo Menczel's anti-cheat module for ioquake3 | Point traces against solid geometry from the viewer's eyes to the 8 corners of a box larger than the player (60 by 100 units by default), the first clear one ending the test; if none, both players are moved 0.1 s ahead by a slide-move simulation and the corners traced again; an optional field-of-view test. An opponent judged invisible is not removed: he is moved straight below the viewer at the same distance, "so that sound scaling will work correctly", and put back after the snapshot. Off by default |
| CornerCulling (a research project for CS:GO) | primary: its README | Rays against hand-placed convex occluders in a bounding volume hierarchy, after the PVS; the line of sight checked "from all locations they could have moved to" during the latency, a rectangle normal to the line of sight covering every peek; culling every 30 ms instead of 10 gives a 3 times speed-up for a delay small against a ping of 100 ms |

What they share: the line of sight is tested from where the viewer's camera can be, to a target enlarged by his motion over the latency; a player is sent before he can be seen; the test is cached or precomputed, since rays every tick for every pair cost too much; audible players remain known to a cheat.

## 5. Measured here

A dedicated server built by clang on the development machine, `mp/ffa3`, 31 bots and one client connected through the system's network, four samples of 25 s (a temporary command timed 200,000 point traces between random pairs of players, eyes to a corner of the target's box; removed after):

| | measure |
|---|---|
| bytes per snapshot for the client | 90.8 (60 s), 9.1 KB per second, against 2,000 bytes per snapshot at the fixed rate |
| the server's time per tick | 409 to 481 microseconds, 4.1 to 4.8 % of one core (the bots' frames 285 to 328) |
| pairs of players in each other's PVS | 218 to 264 of 812 to 930 ordered pairs (27 to 28 %) |
| a point trace (`CM_BoxTrace`, no box, `CONTENTS_SOLID`) | 1.9 to 3.3 microseconds under that load (0.65 to 0.85 with 2 to 7 players); 88 to 91 % of the random traces blocked |

From these, estimated, not measured: a ray test of every pair in the PVS at every tick, with 8 to 30 rays a pair (the hidden pairs need all of them), costs 4 to 20 ms per tick, 10 to 50 times the server's present tick; the VPS is slower than this machine.

## 6. The options

Common to every option, from the conditions set:

- **Where the viewer can see from**: his eyes, and the camera's reach: the range plus the height, from the head, the camera staying in the head's sight (the client traces it so). This needs a bound the server knows: the four camera settings clamped by cgame to limits the server also uses (for example a range of at most 80 to 120 and a height of at most 16 to 32; the limits are a decision). A modified client may move its camera further, but sees only what was sent. To that, the viewer's own motion until the snapshot is drawn: his speed times about his round trip plus a tick, since his own player is drawn ahead of the others by about that.
- **What is tested of the target**: his box, enlarged by the saber's reach when it is on, and by his speed times two ticks, so that a player is sent a tick before he can be seen and the client already holds two snapshots of him when he appears (the others are drawn between two snapshots, part 6).
- **What is never culled**: teammates in team modes; the players a client with Force Sense level 2 or more sees through walls (the PVS, as today); the players a spectator follows sees, from that player's view; the entities that are not players.
- **What a hidden player is**: not sent, as in VALORANT; the client removes him, as when he leaves the PVS today.

| option | CPU per tick (estimated) | precision | work |
|---|---|---|---|
| A: rays at run time, from a few points of the viewer's reach to the corners of the target's enlarged box, stopped at the first clear ray; a hidden result reused while neither player moved more than a margin from where it was tested (16 to 32 units, every 5 to 10 ticks at running speed), a visible one kept for about 100 ms | 0.5 to 4 ms with 32 players on this machine, 1 to 9 times today's tick; more on the VPS; to measure with a prototype | as exact as the margins; a blocked ray is exact | medium: the server's snapshot builder, the reach of the viewer, the cache |
| B: a table per map computed once (VALORANT): the playable space in cells, a cell seeing another if any point of one may see any point of the other, pruned by the PVS; at run time the cells of the viewer's reach against the cells of the target's box | a few microseconds | optimistic by up to a cell: with cells of 64 units a player is sent up to about two players' widths before he can be seen | large: the cells, a builder (minutes per map, estimated, at the first load or by a tool, kept in a file), the table's memory (megabytes per map, estimated), the doors kept to the areas' test the server already does |
| C: B for the clear cases, A's rays only for the pairs of cells on the edge | between B and A | as A | the largest |

The sounds, a decision of their own:

| option | what an honest player hears | what a cheat learns |
|---|---|---|
| S1: a hidden player sends nothing | no footsteps, hum or shots from behind walls: a change of the game | nothing |
| S2: a hidden player within hearing distance sends his position and his sounds only, not drawn | as today | the position of every audible player, as in VALORANT and Counter-Strike: a culled player is then protected only while silent (still, walking, crouched) |
| S3: as S2 with the position rounded (for example to 64 units) | sounds placed within the rounding, hardly heard | a coarse position of the audible players |
| S4: as Enemy Territory: Legacy, a hidden player moved to a false place at the right distance | the right loudness from a wrong direction | a false position |

## 7. Decisions

Taken on 15 November 2026: no update rate by distance (it costs precision on the players far away); no priority per client (it would act only on a snapshot over its budget of 2,000 bytes, and step 9 already keeps the nearest entities when a message does not fit); interest culling wanted against wallhacks, exact, revealed a little early, with the camera.

Taken later the same day, after the options above:

1. The test: B at first sight, a table per map, since it costs nearly no CPU at run time; C, which adds rays for the pairs on the edge of the table's cells, only if B's early reveal (up to a cell) proves too generous. Neither may lower the precision or the responsiveness of the game: a player is never sent late, only early. A prototype measures B's build time and memory on two or three maps (and the cell size that keeps the early reveal to the edge of a wall) before it is written.
2. Where the table comes from: a file in a pk3 when the map has one; otherwise computed by the server and kept in a cache; a tool to make the file, for those who want to ship it. How long the computation takes is measured first (the server's choice of computing it in the background while the map plays with today's PVS is to decide then). Revisited after the prototype (the same evening): no computation on a server, which is often a single virtual core where the game must not be slowed; the file is made beforehand by the tool, on any machine, and shipped in the map's pk3 or placed by the administrator; without it, today's PVS (section 9).
3. The camera's limits: a range of at most 120 and a height of at most 32 (the defaults 80 and 16 unchanged), enforced by cgame and used by the server. The angle and pitch offsets need no limit: they turn the camera within the same sphere around the head. Revised after the prototype (the same evening), as competitive third-person games do (the camera set by the game, the same for everyone): the camera fixed at the defaults, a range of 80, a height of 16, no angle or pitch offset (`cg_thirdPersonRange`, `cg_thirdPersonVertOffset`, `cg_thirdPersonAngle`, `cg_thirdPersonPitchOffset` no longer settings). The field of view (`cg_fov`) is not concerned: it widens or narrows the picture from the same camera position, and the culling uses no field of view.
4. The sounds: S2. A player one can hear stays heard exactly as today (players guess who is coming by the sound, and recent games keep it so); a hidden player within hearing distance is sent with his position and his sounds, not drawn by the client. His position is then in the packets, so a cheat can mark him: only the silent players are hidden from a wallhack, as in VALORANT and Counter-Strike, which publish nothing more against that and rely on their client-side anti-cheats (Vanguard, VAC) for it (not verified further). Sending a hidden player only while one of his sounds plays was looked at and not kept: a sound follows its entity's position while it plays.
5. Spectators: no limit; a spectator can follow any player, and sees everything as today.
6. The server's demos: every player of the PVS, with no culling, for an administrator's review: the demo records the list of entities as it is before the culling (the demo already has its own frames, so only that list is kept for a recorded client). A client's own demo can only hold what the client was sent.

## 8. The prototype (15 November 2026)

Measured with temporary server commands and offline tools ([../tools/visibility/](../tools/visibility/README.md)), each removed from the engine after its measure. A dedicated server built by clang on the development machine; 31 bots of skill 4 with the test servers' settings (sabers, force jump); 60 instants of a game per map; at each, every ordered pair of players in the game (55,800 pairs on `mp/ffa2`, `mp/ffa3` and `mp/ctf5`, 20,460 on `mp/siege_desert`, where fewer bots join). For each pair: sent today (the PVS of the eye and the areas), sent with the table, and the truth: a clear segment from the eye, from the camera's target (the head raised by 32) or from 26 camera positions at 120 units (traced as cgame traces the camera) to the corners, the centre or the eye of the target's box. A table that says hidden where the truth says visible is a miss: a player not sent whom the client could see. The truth is itself sampled (28 by 10 segments) and takes the camera at its bounds in every direction.

At run time the table is read for the viewer's cell and the cells he can move into before the snapshot is drawn (his speed times 110 ms), against the cells the target's box touches, widened by the saber's reach (50) and two ticks of the target's movement; a viewer or a target whose position is in no cell is sent.

### 8.1 A table of every open cube: the first attempt

The map cut into cubes of 128 units; two cubes see each other if a segment between their corners is clear, traced by the engine's collision (`CM_BoxTrace`) and blocked only by an opaque surface that is drawn (not glass, `trans` or `nodraw` surfaces, except terrain).

| map | cubes | build, one core | table | today (PVS) | with the table | with the table widened by a cube | visible | misses (widened) |
|---|---|---|---|---|---|---|---|---|
| `mp/ffa3` | 18,882 | 90 s | 42.5 MB | 32.2 % | 28.5 % | 32.1 % | 16.6 % | 69 (1) |
| `mp/ffa2` | 21,969 | 45 s | 57.5 MB | 23.3 % | 20.4 % | 22.7 % | 13.7 % | 14 (4) |
| `mp/ctf5` | 42,421 | 191 s | 214.5 MB | 69.0 % | 41.9 % | 44.0 % | 35.1 % | 97 (32) |

It failed on three counts: misses (a cube astride a wall has its corners on one side and the player on the other), a gain that vanishes once the table is widened enough to remove them, and a cost that grows with the square of the cubes (at 64 units on `mp/ffa3`: 111,089 cubes, a table of 1.47 GB, about an hour; at 128 units, an estimated 39 minutes and 259 MB for `mp/siege_hoth`).

Seen on the way, and corrected later the same day: with the engine's traces as the truth, 19 to 383 pairs of 55,800 seemed visible from the camera and outside the eye's PVS, players the base game would not send. With the truth on the drawn triangles (section 8.4: the engine's traces pass through some solid patches and through the models compiled into the map), only 5 pairs of 55,800 on `mp/ffa3`, 1 on `mp/ctf5` and none on `mp/siege_desert` are visible and outside today's PVS.

### 8.2 Where a player can be: the walkable layer, from the map alone

The player's box (30 by 30 by 64) traced down every 16 units finds each place where it rests (a slope of 0.7 at most, inside the world); above it, the ceiling, up to the highest jump (426: a force jump of level 3 with the step and the crouch). From the spawn points, a flood follows walking (steps of 18), jumping up to that ceiling and falling between neighbouring columns. Cells: a floor cell per footprint of 64 by 64 and per level of floor (the places of a footprint grouped by height), from the feet to the head plus a normal jump; air cells of 128 above, up to the ceiling or the highest jump. 0.4 to 8.6 s per map with the engine's collision.

| map | resting places | reached | floor cells | air cells |
|---|---|---|---|---|
| `mp/ffa2` | 167,130 | 52,426 | 4,132 | 3,735 |
| `mp/ffa3` | 135,039 | 38,323 | 3,146 | 2,630 |
| `mp/ctf5` | 237,579 | 68,626 | 5,751 | 4,736 |
| `mp/siege_desert` | 2,098,716 | 422,450 | 32,438 | 30,885 |

The bots' positions fall in a cell 99.5 % of the time on `mp/ffa3`, 99.8 % on `mp/ctf5` and 96.2 % on `mp/siege_desert` (120 instants, the same game for every variant). Tried and not kept: jumps over a gap (16 directions, up to 384 units), the teleporters' destinations and the push targets as seeds, steep slopes as places: at most one point more of coverage, for 6 to 11 times the layer's time or 12,000 more cells. What stays outside is players in mid-jump over a drop, 600 to 1,000 units above the floor below them, after leaving a high ledge: brief, and sent.

### 8.3 The camera in the table

Each cell's samples are real places of the layer: two per floor cell (the most central, then the farthest from it), at the body's centre and at the eye. With the camera reached through the cells around the viewer (a symmetric table, the cells within 152 units that the viewer's cell sees), the misses come from the camera: on `mp/ctf5`, 155 of 169 seen only from a camera position between 80 and 120 units, where an air cell has a single sample. With each cell's own camera positions in its row (32 directions from the head, traced as cgame does), they fall by about three (`mp/ffa2` from 12 to 1, `mp/ffa3` from 32 and 59 in two games to 24, `mp/ctf5` from 169 to 53), and the row already holds the camera, so the run time reads one row per viewer; but the build then costs 9 to 56 minutes of the engine's tracer per base map (in 16 processes, 37 to 219 s).

### 8.4 The tracer

The engine's collision traces brushes through the BSP tree, a box or a point, made for movement. A bounding volume hierarchy over the drawn triangles (the BSP's draw surfaces: planar faces, patches tessellated, models compiled into the map; without the shaders that are not opaque) answers a segment's occlusion only. Recorded segments of the table (1 to 4 million per map), the time per segment on one core:

| map | triangles | engine's collision | a scalar BVH written here | tinybvh `BVH4_CPU`, SSE4.2 | tinybvh `BVH4_CPU`, AVX2 | madmann91/bvh v2 |
|---|---|---|---|---|---|---|
| `mp/ffa2` | 23,886 | 576 ns | 126 ns | 47 ns | 43 ns | 112 ns |
| `mp/ffa3` | 130,490 | 1,747 ns | 141 ns | 57 ns | 52 ns | 139 ns |
| `mp/ctf5` | 149,993 | 1,155 ns | 169 ns | 72 ns | 68 ns | 169 ns |
| `mp/siege_desert` | 33,863 | 1,164 ns | | 44 ns | | |

tinybvh (MIT, one header) builds its hierarchy in 8 to 18 ms; its 8-wide layout is no faster here; its SSE4.2 build is within 10 % of AVX2, so a processor without AVX2 loses little. madmann91/bvh v2, built with FMA contraction, let through 3,484 segments on `mp/ffa3` and 16,666 on `mp/ctf5` that the triangles block: not kept. Embree, the reference, is under the Apache 2.0 licence, incompatible with the GPL v2: not considered.

The verdicts differ from the engine's on 0.003 to 0.09 % of the segments. Where the triangles block and the engine does not, the surfaces are drawn and opaque: on `mp/ctf5`, 1,919 of 3,602 such segments cross the pillars of `textures/factory/piller` (patches, contents solid and opaque, a first stage without blending) that the engine's point traces pass through; the rest are walls without scripts and models compiled into the map. The triangles are the right geometry for what a player sees. The engine's traces through those patches are noted for plan step 13.

### 8.5 A shared lattice of camera points, and a fan of rays for the far

Two changes take the build from minutes to seconds:

- **A lattice of camera points shared by the cells.** Above each floor cell, points at the eye, 64 and 128 above it (under the ceiling), and the eye of its second place; each air cell's point. For each cell, the lattice points its camera reaches (within 152 units of the head, a box of 4 traced from the head as cgame does): 41 to 44 on average. A cell's row is the union of what its lattice points see, and each lattice point is judged once per target cell.
- **Near and far.** Within 2,048 units (between the cells' boxes), the rows are exact to the cell: a ray from a lattice point to one of the target cell's samples. Beyond, a fan of 1,024 rays from each lattice point marks the blocks of 512 units it crosses, widened where the rays spread more than a block apart; a far target is sent if its block was crossed.

Traced on the drawn triangles with tinybvh (SSE4.2), one core of the development machine; the truth judged again on the same triangles; the run-time rule that a target whose own position is in no cell is sent (without it, a target outside the layer was judged on the cells his widened box touched, which caused most of the misses on `mp/siege_desert`):

| map | layer (engine) | table (one core) | compact size (near bits, far rows) | today (PVS) | PVS and table | areas and table | visible | misses (PVS and table / areas and table) | from the eye | from a camera beyond 80 |
|---|---|---|---|---|---|---|---|---|---|---|
| `mp/ffa2` | 0.4 s | 6.1 s | 4.6 MB | 25.7 % | 18.9 % | 25.5 % | 13.8 % | 1 / 4 | 0 | 1 |
| `mp/ffa3` | 0.8 s | 4.6 s | 2.6 MB | 31.8 % | 25.1 % | 30.2 % | 18.0 % | 3 / 3 | 0 | 3 |
| `mp/ctf5` | 1.2 s | 15 s | 6.4 MB | 67.5 % | 32.3 % | 32.7 % | 26.7 % | 15 / 15 | 0 | 15 |
| `mp/siege_desert` | 8.2 s | 95 s | 63 MB | 100 % | 91.9 % | 91.9 % | 36.4 % | 10 / 10 | 8 | 2 |

"PVS and table" keeps today's test from the eye and adds the table to it; "areas and table" puts the table in place of the PVS, the areas (doors) kept. Counted over the same pairs, with the truth on the drawn triangles:

| map | hidden players sent (a wallhack shows them): today | PVS and table | areas and table | visible players not sent: today | PVS and table | areas and table |
|---|---|---|---|---|---|---|
| `mp/ffa3` | 7,718 | 3,946 | 6,824 | 5 | 8 | 3 |
| `mp/ctf5` | 22,778 | 3,124 | 3,388 | 1 | 16 | 15 |
| `mp/siege_desert` | 13,017 | 11,376 | 11,376 | 0 | 10 | 10 |

In place of the PVS the table recovers today's few visible pairs outside the eye's PVS (5 on `mp/ffa3`), but sends 73 % more hidden players there than with it: the table's margins (the cells, the target's box widened, the camera at 120) reveal more than the eye's PVS on a closed map.

The `mp/ffa2` line is from the run before the rule (the rule can only add pairs). The misses are counted against the truth on the drawn triangles. The run time costs 5 to 7 microseconds per viewer and his targets, without a cache. Earlier tables compressed 10 to 25 times; this format was not compressed.

By distance, the far part works on closed maps and not on open ones: beyond 2,048 units `mp/ffa3` sends 2,430 pairs of 24,333 for 542 visible, `mp/ctf5` 479 of 31,103 for none, `mp/siege_desert` about 90 % for 7 %. A block is a cube: a ray passing high over a distant floor marks the whole block, the floor hidden behind a dune included; blocks of 256 and 2,048 rays per point changed nothing (87.8 % sent, 112 s). `mp/siege_desert` has no useful PVS (every pair is in it).

### 8.6 What the prototype leaves open

- The misses from camera positions between 80 and 120 units: 0.01 to 0.1 % of the visible pairs; a camera bounded at 80, the default, would remove them (decision 3 set 120).
- The far part on open maps: marking the floor cells a ray actually passes through, at the height of a body above the floor, instead of blocks; estimated at minutes on open maps (long rays), not prototyped.
- `mp/siege_desert`: 95 s on one core of the development machine, and 8 misses from the eye at a few places with floors on several levels.
- 27 % of the pairs on `mp/siege_desert` had a player outside the layer in one game (mostly jumps over drops): sent, so no loss, but no culling for them.

## 9. The design proposed (to decide)

From the prototype, to implement after the decisions:

- **The file.** Made beforehand by a tool, on any machine and on all its cores, never by a server (15 November 2026): the tool is a command of the dedicated server, so that it uses the engine's own collision for the layer and the same code on Windows and Linux. The file is shipped in the map's pk3 or placed by the administrator; it holds the BSP's checksum, and a server refuses a file that does not match the map; it is compressed with zlib-ng, already vendored for the pk3 files (the same day). Without a file: today's PVS.
- **Its content.** The layer's cells (floor cells by footprint of 64 and level, air cells of 128), the near rows (a bit per pair of cells within 2,048 units, in an order recomputed from the cells) and the far rows (a bit per block of 512): 2.6 to 6.4 MB on the base maps before compression, 63 MB on `mp/siege_desert`.
- **The build.** The layer by the engine's collision (0.4 to 8 s); the shared lattice of camera points; the near rows by segments and the far rows by fans, traced on the drawn opaque triangles with tinybvh (MIT, vendored at a pinned commit), SSE4.2 everywhere (AVX2 gains under 10 %). 5 to 16 s per base map and 95 s for `mp/siege_desert` on one core of the development machine, divided by the cores the tool is given.
- **At run time.** For each client, the union of the rows of his cell and of the cells he can move into before the snapshot is drawn, kept until he changes cell; a target is sent if one of the cells his widened box touches is in it; a viewer or a target whose position is in no cell is sent; then today's PVS of the eye and the areas, or the areas alone (to decide, section 8.5). Unchanged from section 7: teammates, Force Sense from level 2, spectators and the server's demos are not culled; a hidden player one can hear is sent with his sounds, not drawn.
- **A switch.** A server setting turns the culling off, back to today's PVS, whether a file exists or not (15 November 2026).
- **The size.** The exact marking of the far part as a full table of cells would weigh 478 MB on `mp/siege_desert` before compression: too much. The far part keeps a coarse unit, to choose by measure.
- **To decide.** The table with today's PVS (half the hidden players sent on `mp/ffa3`) or in its place (today's few visible pairs outside the eye's PVS recovered, 73 % more hidden players sent there; section 8.5); the camera's damping (`cg_thirdPersonCameraDamp`, default 0.3, and `cg_thirdPersonTargetDamp`, default 0.5, a player's setting: a camera that lags far behind a running player leaves the sphere the table assumes; proposed: fixed at the defaults as the rest of the camera); the far part's unit (section 8.6; proposed: blocks on the floor, a footprint of 256 by level of floor, marked only where a ray passes at the height of a body above that floor, about the size of today's far rows, an estimated 20 to 25 MB before compression on `mp/siege_desert`, to measure). The choice comes later; the implementation waits for these choices (15 November 2026).

## 10. What was not read or verified

- VALORANT's cell size, its look-ahead time and how it treats audible enemies are not published; CS2's implementation neither.
- `sv_occlude_players`' method in CS:GO is known from forum posts only.
- The CPU figure of option A is an estimate from the measured trace cost; option B was prototyped (section 8). The engine's trace cost moved by a factor of 2 to 5 with the load of the machine.
- The measures of section 5 are on one map (`mp/ffa3`); the prototype measured four maps, with bots, not human players; the build was timed on the development machine, not on a server's core.
- `mp/siege_desert` has no bot routes (no `botroutes/mp/siege_desert.wnt`, where `mp/ffa2`, `mp/ffa3` and `mp/ctf5` have theirs): its bots wander and jump most of the time, so its measures say less about a real game than the others.
- The prototype's truth is sampled (28 by 10 segments per pair) and puts the camera at its bounds in every direction; a miss it does not see is possible.
- The engine's point traces through the solid curved patches of section 8.4 were not tested in game.
- The hearing distance of JKA's sounds was not read.

## 11. Sources

- Riot Games, "Demolishing Wallhacks with VALORANT's Fog of War": https://www.riotgames.com/en/news/demolishing-wallhacks-valorants-fog-war
- Enemy Territory: Legacy, `src/server/sv_wallhack.c`: https://github.com/etlegacy/etlegacy/blob/master/src/server/sv_wallhack.c ; its anti-cheat notes, `docs/game/anticheat.html`: https://github.com/etlegacy/etlegacy/blob/master/docs/game/anticheat.html
- The ioquake3 anti-cheat module (Laszlo Menczel), mirror: https://github.com/lrq3000/ioquake3-anti-cheat
- CornerCulling: https://github.com/87andrewh/CornerCulling
- AlliedModders, anti-wallhack for CS:GO: https://forums.alliedmods.net/showthread.php?t=291274 and https://forums.alliedmods.net/showthread.php?p=2512120
- PepperEyes, CS2 server-side occlusion culling: https://peppereyes.com/personal-safety/cs2-fog-of-war-server-sided-anti-wallhack-occlusion-culling-for-cs2-servers/
- GameRiv, a CS2 server-side anti-wallhack plugin: https://gameriv.com/cs2-modder-builds-a-server-side-anti-wallhack-that-could-also-boost-your-fps/
- Hacker News, why the culling must be on the server: https://news.ycombinator.com/item?id=41938847
- tinybvh (Jacco Bikker, MIT): https://github.com/jbikker/tinybvh ; its manual: https://jacco.ompf2.com/2025/01/24/tinybvh-manual-basic-use/
- madmann91/bvh (MIT): https://github.com/madmann91/bvh
- Embree (Apache 2.0): https://github.com/RenderKit/embree
