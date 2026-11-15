# Not sending the players a client cannot see: what recent games do, what JKA shows, the measures and the options against wallhacks

Written on 15 November 2026, for plan step 14 part 11 ([../PLAN.md](../PLAN.md); [../progress/step-14-fixed-tick.md](../progress/step-14-fixed-tick.md) section 5). A wallhack draws the players the client was sent, wherever they are; the only defence that does not rely on the client is the server not sending a player the client cannot see. The owner wants it, on one condition: it must be exact, with no loss of sight and no player shown late; a player may be revealed a little early, so that a wallhack shows only the very edge of a wall; the camera must be taken into account. Update rates by distance and priorities per client were set aside the same day (section 7).

Each source is labelled as in the other surveys:

- **primary**: the studio's own article, talk slides or source code, or the engine's or library's own documentation;
- **secondary**: a summary written by someone else;
- **community**: forum posts, player guides, pull requests of forks.

## 1. Summary

1. Today the server sends a client every entity in the potentially visible set (PVS) of its eyes, the map's coarse cluster visibility: a player behind a wall in a visible cluster is sent, so a wallhack sees him.
2. Recent games test the line of sight on the server and send a player shortly before he can become visible: VALORANT from a precomputed cell-to-cell table (less than 2 % of its server frame), Counter-Strike with an occlusion test, Enemy Territory: Legacy (the same engine family) with point traces to the corners of a box. They all reveal early to cover the latency and accept that a player audible or about to appear is known to a cheat.
3. In JKA the camera is behind the player (third person is forced with the saber), its distance and height are the player's own settings, Force Sense level 2 draws players through walls, sounds come from the players' entities, and a saber blade reaches past a corner before its owner.
4. Measured here with 32 players on `mp/ffa3`: 91 bytes per snapshot for a client, 4.1 to 4.8 % of a core per tick for the server, 27 % of the pairs of players in each other's PVS, a point trace 1.9 to 3.3 microseconds under that load.
5. Tracing rays every tick for every pair would cost several times the server's present tick; a test reused while the players stay within a margin, or a table computed per map, is needed. Nothing is decided; the options and the decisions are in sections 6 and 7.

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

A dedicated server built by clang on the owner's machine, `mp/ffa3`, 31 bots and one client connected through the system's network, four samples of 25 s (a temporary command timed 200,000 point traces between random pairs of players, eyes to a corner of the target's box; removed after):

| | measure |
|---|---|
| bytes per snapshot for the client | 90.8 (60 s), 9.1 KB per second, against 2,000 bytes per snapshot at the fixed rate |
| the server's time per tick | 409 to 481 microseconds, 4.1 to 4.8 % of one core (the bots' frames 285 to 328) |
| pairs of players in each other's PVS | 218 to 264 of 812 to 930 ordered pairs (27 to 28 %) |
| a point trace (`CM_BoxTrace`, no box, `CONTENTS_SOLID`) | 1.9 to 3.3 microseconds under that load (0.65 to 0.85 with 2 to 7 players); 88 to 91 % of the random traces blocked |

From these, estimated, not measured: a ray test of every pair in the PVS at every tick, with 8 to 30 rays a pair (the hidden pairs need all of them), costs 4 to 20 ms per tick, 10 to 50 times the server's present tick; the VPS is slower than this machine.

## 6. The options

Common to every option, from the owner's conditions:

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

## 7. Decisions for the owner

Taken on 15 November 2026: no update rate by distance (it costs precision on the players far away); no priority per client (it would act only on a snapshot over its budget of 2,000 bytes, and step 9 already keeps the nearest entities when a message does not fit); interest culling wanted against wallhacks, exact, revealed a little early, with the camera.

To take:

1. The test: A, B or C, after a prototype has measured A's CPU with 32 players (here and on the VPS) and B's build time and memory on two or three maps.
2. The camera's limits the server uses and cgame enforces: range, height, angle and pitch offsets (today 80 and 16 by default, no bound).
3. The sounds: S1 to S4.
4. Spectators not following anyone: everything, as today (a spectator could tell a player where the others are), or the culling of the spectator's own view.
5. The server's demos (`svrecord`, `sv_autoDemo`): what the client was sent, culled, or every player in the PVS for an administrator's review (another set of entities to build per recorded client).

## 8. What was not read or verified

- VALORANT's cell size, its look-ahead time and how it treats audible enemies are not published; CS2's implementation neither.
- `sv_occlude_players`' method in CS:GO is known from forum posts only.
- The CPU figures of options A and B are estimates from the measured trace cost; nothing was prototyped. The trace cost moved by a factor of 2 to 5 with the load of the machine.
- One map (`mp/ffa3`); the pairs in the PVS depend on the map.
- The hearing distance of JKA's sounds was not read.

## 9. Sources

- Riot Games, "Demolishing Wallhacks with VALORANT's Fog of War": https://www.riotgames.com/en/news/demolishing-wallhacks-valorants-fog-war
- Enemy Territory: Legacy, `src/server/sv_wallhack.c`: https://github.com/etlegacy/etlegacy/blob/master/src/server/sv_wallhack.c ; its anti-cheat notes, `docs/game/anticheat.html`: https://github.com/etlegacy/etlegacy/blob/master/docs/game/anticheat.html
- The ioquake3 anti-cheat module (Laszlo Menczel), mirror: https://github.com/lrq3000/ioquake3-anti-cheat
- CornerCulling: https://github.com/87andrewh/CornerCulling
- AlliedModders, anti-wallhack for CS:GO: https://forums.alliedmods.net/showthread.php?t=291274 and https://forums.alliedmods.net/showthread.php?p=2512120
- PepperEyes, CS2 server-side occlusion culling: https://peppereyes.com/personal-safety/cs2-fog-of-war-server-sided-anti-wallhack-occlusion-culling-for-cs2-servers/
- GameRiv, a CS2 server-side anti-wallhack plugin: https://gameriv.com/cs2-modder-builds-a-server-side-anti-wallhack-that-could-also-boost-your-fps/
- Hacker News, why the culling must be on the server: https://news.ycombinator.com/item?id=41938847
