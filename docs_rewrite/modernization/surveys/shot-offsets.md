# Where a shot starts against the view

Plan step 20, part 9 (decided on 20 November 2026, after part 5; its decisions taken on 20 November 2026 before the part: [../PLAN.md](../PLAN.md) step 20). For every shooter of the multiplayer game (the player's weapons, the emplaced gun, the E-Web, the vehicles, the NPCs, the bots, the turrets, the sentry, the seeker, the saber), this survey records where the shot starts, in which direction, what the crosshair shows, the offset between them and the view, why, and whether it is normal. It changes nothing in the game; the defects it shows are listed in section 11, each to be decided.

Every reading of the code below is the same in 1.01 (`E:\Git\jka\jediacademy\codemp`) unless said otherwise. The numbers come from temporary measuring code kept as [../tools/gamestats/shot_offsets.patch](../tools/gamestats/shot_offsets.patch), run by [../tools/gamestats/shot_offsets.sh](../tools/gamestats/shot_offsets.sh) (section 12).

Units are the game's (a player's box is 32 units wide and 64 high when standing). "From the eyes" means from the player's origin raised by its view height; "forward, right, up" are along the view.

## 1. Summary

| shooter | where the shot starts | direction | what the crosshair shows (`cg_dynamicCrosshair 1`, the default) | offset from the view | normal? |
|---|---|---|---|---|---|
| the player's projectile weapons, the concussion rifle's beam | the eyes plus a fixed point per weapon: 12 forward, 0 to 8 right, 4 to 10 down (`WP_MuzzlePoint`) | the view's, parallel: never towards the point aimed at | the end of a ray from that point along the view: where a straight shot lands | 2 to 8 units right, 4 to 6 down, the same at every distance | for a player, yes: the crosshair tells the truth; see sections 3 and 11 |
| the disruptor (both fires) | the eyes | the view's | from the eyes (cgame clears the offset) | none | yes |
| the stun baton, the melee | 20 forward, 4 right, 6 down | the view's | from the eyes | 4 right, 6 down, over a reach of a few dozen units | yes |
| the thermal detonator, the trip mine, the det pack | the weapon's fixed point (12 forward, 0 right, 4 or 10 down) | thrown along the view (a ballistic arc for the detonator) | the straight ray from that point | an arc, not an offset | yes |
| the emplaced gun | 10 units left or right of a point 46 above the gun, in turn | the view's, its pitch capped at 40 degrees down | from the point between the two barrels | 10 units to the side of the crosshair's point | **no: the only crosshair that misleads** (section 4) |
| the E-Web | the cannon's muzzle on its model (`*cannonflash`) | the cannon's: 4.56 degrees above and 1.10 to the left of the view at rest, and 0.8 of the view's pitch | the cannon's axis: the same line as the shot (measured) | the cannon does not point where the view looks | for the crosshair, yes; the centre of the screen is not the cannon (section 5) |
| the vehicles | the muzzles on the vehicle's model | the AT-ST and the X-wing converge on the crosshair's point; the swoop fires along its heading | the AT-ST and the X-wing from the vehicle; the swoop's rider from its own view | swoop: 43 below the rider's eyes, level while the rider looks down | the swoop's crosshair does not follow its guns (section 6) |
| the NPCs that shoot (the stormtrooper) | the player's fixed point | towards the target's eye level from the NPC's eyes, then fired parallel from the fixed point | none | 6 right, 6 down from the line aimed | **no: aimed from the eyes, fired from the muzzle** (section 7) |
| the bots | the player's fixed point | aimed from their eyes | none | the same | **no**, the same (section 7) |
| the turrets, the sentry, the seeker | their own gun | aimed from their gun at the target | none | none against their aim | yes (section 8) |
| the saber's blade | the blade of the model | the animation | none | no view involved | yes |
| the thrown saber | the saber's hilt in the hand (11 forward, 7 right, 24 down) | the view's at level 1; at levels 2 and 3 steered towards the point the eyes look at | none | at level 1, 7 right and 24 down | yes (section 9) |

## 2. How the crosshair is drawn

cgame draws the crosshair where a ray hits (`CG_ScanForCrosshairEntity`, `cgame/cg_draw.cpp`). With `cg_dynamicCrosshair 1` (the default, archived) the ray starts at the weapon's fixed point computed as the server does (`CG_CalcMuzzlePoint`, `cgame/cg_weapons.cpp`) and runs along the view; with `cg_dynamicCrosshairPrecision 1` (the default) it is traced against the models as the shots are (`CG_G2Trace`). So the crosshair moves on the screen and is drawn where a straight shot lands. With `cg_dynamicCrosshair 0` the ray starts at the camera and the crosshair stays at the centre of the screen.

- `CG_CalcMuzzlePoint` clears the fixed point for the disruptor, the stun baton, the melee and the saber, whose shots start at the eyes or have no crosshair shot.
- In third person the ray starts from the predicted player's origin with the view height, as the server's; in first person from the camera (`cg.refdef.vieworg`), the fixed point's height added without the view height. The camera in first person carries the view's bobbing and the landing and step offsets, which the server's point does not: the crosshair can differ from the shot by these few units in first person (read from the code, not measured).
- The emplaced gun: the ray starts 46 above the gun's origin, between the two barrels (section 4). The E-Web: from the cannon's muzzle along its axis (`CG_CalcEWebMuzzlePoint`, section 5). A vehicle that hides its rider (`hideRider 1`: the AT-ST, the X-wing): from the vehicle's muzzle (`CG_CalcVehicleMuzzlePoint`); the swoop does not hide its rider, so its crosshair is the rider's own (section 6).

## 3. The player's weapons

`CalcMuzzlePoint` (`game/g_weapon.cpp`) starts every shot at the player's origin raised by its view height, then moves it by the weapon's row of `WP_MuzzlePoint` (`game/bg_weapons.cpp`), forward and right along the view and straight up:

| weapon | forward | right | up |
|---|---|---|---|
| stun baton, melee | 0 | 8 | 0 |
| saber | 8 | 16 | 0 |
| bryar, blaster, disruptor, DEMP2, flechette, concussion, old bryar | 12 | 6 | -6 |
| bowcaster | 12 | 2 | -6 |
| repeater | 12 | 4.5 | -6 |
| rocket launcher | 12 | 8 | -4 |
| thermal detonator, det pack | 12 | 0 | -4 |
| trip mine | 12 | 0 | -10 |

The shot then flies along the view (`forward`), parallel to the line from the eyes: it never turns towards the point the eyes aim at. The point is meant to sit at the weapon, so that the bolt seems to leave the gun rather than the eyes; Raven's comment on `CalcMuzzlePoint` notes that the weapon's real bolt "may be reasonable" instead, with the drawback that it "does not necessarily look alright from a first person perspective". Quake III Arena starts every shot 14 units in front of the eyes, on the view's line (section 10); Jedi Academy added the side and height offsets.

Exceptions in the weapons' own functions: the disruptor's two fires start at the eyes (`WP_DisruptorMainFire`, `WP_DisruptorAltFire`); the stun baton and the melee trace from 20 forward, 4 right and 6 down (`WP_FireStunBaton`, `WP_FireMelee`); the concussion rifle's beam starts at the fixed point (`WP_FireConcussionAlt`); the thermal detonator, the trip mine and the det pack are entities thrown from the fixed point (not measured: they do not go through `CreateMissile`).

**Measured** (`shot_offsets.sh`, batch a): the defender standing 256 units away, holding no saber, its head (the bolt `*head_eyes` of its model) aimed at from the eyes, as a bot, an NPC or a player with `cg_dynamicCrosshair 0` aims, then from the weapon's point, as the default crosshair shows; the spread left out; 8 repetitions; the hits by location as `G_LocationBasedDamageModifier` counts them.

| fire | the line passes from the head point, aimed from the eyes | hits aimed from the eyes | hits aimed from the weapon's point |
|---|---|---|---|
| bryar (point bolt) | 6 right, 6 down | 16 of 16, head | 16 of 16, head |
| blaster (point bolt) | 6 right, 6 down | 24 of 24, head | 24 of 24, head |
| repeater (point bolts) | 4.5 right, 6 down | 48, head | 48, head |
| old bryar (point bolt) | 6 right, 6 down | 24 of 24, head | 24 of 24, head |
| bowcaster, one bolt (box of 4) | 2 right, 6 down | 8 of 8, back | 8 of 8, head |
| DEMP2 (box of 4) | 6 right, 6 down | 16 of 16, right arm | 16 of 16, head |
| flechette (boxes of 2) | 6 right, 6 down | 80, right arm | 56, head |
| rocket (box of 6) | 8 right, 4 down | 16, right arm | 8, head |
| concussion rifle (box of 6) | 6 right, 6 down | 16 of 16, left hand | 16 of 16, head |
| disruptor | none (from the eyes) | 16 of 16, head | **none**: aimed from a point 6 right and 6 down that the shot does not start from, it passes 6 left and 6 above the head |

The repeater's, the flechette's and the rocket's counts stop at the defender's death in each repetition. The point bolts (no box) still cross the head 6 units off its eyes point; the missiles with a box (`G2TRFLAG_THICK`: half sizes 1 to 3, `BOWCASTER_SIZE`, `DEMP2_SIZE`, `FLECHETTE_SIZE`, `ROCKET_SIZE`) touch the shoulder, the arm or the hand first. Aimed at the chest from the eyes, the blaster's 24 bolts hit the body. The disruptor confirms why cgame clears its offset: a crosshair drawn from the fixed point would mislead for it.

**The real barrel in third person.** The bolt starts at the fixed point, the muzzle flash is drawn at the weapon model's `*flash` bolt (`tag_flash`; `cgame/cg_weapons.cpp`). Measured with the weapon's world model put on the attacker's right hand as cgame does (batch b, `flash`), the `*flash` bolt from the eyes, in the two poses the fire animation gives:

| weapon | fixed point (forward, right, up) | `*flash` in third person |
|---|---|---|
| bryar | 12, 6, -6 | 31, 4, -12 and 23, 5, -17 |
| blaster | 12, 6, -6 | 19, 4, -13 and 17, 5, -12 |
| disruptor | 0, 0, 0 (the eyes) | 27, 3, -11 and 25, 5, -10 |
| bowcaster | 12, 2, -6 | 18, 5, -12 |
| repeater | 12, 4.5, -6 | 23, 3, -7 and 23, 3, -6 |
| DEMP2 | 12, 6, -6 | 17, 4, -15 and 15, 6, -14 |
| flechette | 12, 6, -6 | 19, 5, -9 and 21, 4, -10 |
| rocket launcher | 12, 8, -4 | 22, 4, -12 and 20, 4, -12 |
| concussion rifle | 12, 6, -6 | 26, 3, -12 and 25, 4, -11 |
| old bryar | 12, 6, -6 | 36, 5, -13 and 28, 6, -18 |

In third person a bolt therefore appears 5 to 24 units behind and 1 to 12 units above the flash of the gun it seems to leave (the repeater's point is nearest, within 11 units forward and 1 up). The first-person weapon model was not measured.

**The spread** is separate from the offset: a random angle added to the direction at each shot (`game/g_weapon.cpp`), where the offset moves the start the same way every time. The blaster's alternate fire ±1.6 degrees, the repeater's primary ±1.4, the flechette's primary ±4 except its first bolt ("it will hit the crosshairs"), the flechette's alternate ±2 degrees in yaw and 8 to 12 up, the bowcaster's primary a fixed fan in yaw with ±1 degree in pitch and a random speed. At 256 units a degree is 4.5 units. Kept (batch b): with the head aimed at from the weapon's point, the blaster's alternate hits it 29 times instead of 32, the repeater 44 instead of 48, the flechette 20 times in the head and 11 elsewhere, the bowcaster's fan 8 in the head (its side bolts 21 units off).

## 4. The emplaced gun

`WP_FireEmplaced` fires from the gun's origin raised by 46, then 10 units right or left along the view, in turn (`genericValue10`), along the view, its pitch capped at 40 degrees down (`FireWeapon`). cgame's crosshair starts at the gun's origin raised by 46 with the same pitch cap, between the two barrels (`CG_CalcMuzzlePoint`, `CG_ScanForCrosshairEntity`): the table `WP_MuzzlePoint` has no row for the emplaced gun, so nothing is added. So every bolt passes 10 units to the side of the point the crosshair shows. Measured (batch b): from the player's eyes the barrels are 51 forward, 10 right or left and 24 down; aimed at a defender 204 units away, the bolts pass 10 units beside its chest's centre, 8 to 12 beside its head. In step 20 part 2 (19 November 2026) only an aim 2 degrees up hit, 15 bolts in 41.

## 5. The E-Web

The E-Web fires from its model's `*cannonflash` bolt along the bolt's axis (`EWebFire`, `game/g_items.cpp`), 16 units back into its box. The cannon follows the view through two bones (`EWebUpdateBoneAngles`): its yaw turns towards the view's by 4 degrees at most per update (80 degrees a second at the 20 Hz instants), its pitch is 0.8 of the view's against the E-Web's base, which is level. Measured (batch b, a trace of the cannon's axis at each shot): with the view level, the cannon fires 4.56 degrees above it and 1.10 degrees to its left (the model's own axis at rest); with the view 2.72 degrees down, the shot is 2.38 degrees up, as 0.8 of the pitch gives. The axis cgame's crosshair takes (`CG_CalcEWebMuzzlePoint`: the bolt matrix without its model rotation, its negative X) is the shot's (the server's: with the rotation, its negative Y) to 0.01 degree, from the same point: the crosshair tells the truth.

So a player sees where the cannon fires, but the crosshair is not at the centre of the screen and does not move one for one with the view in pitch. A bot or a view aimed at the defender's chest from the eyes (batch b): the shots pass 6 units above the chest and hit the head (32 of 80 shots); with the view level, as step 20 part 6's scenario, they pass above it (8 hits in 80), which is why "most shots miss" there. The first shot of a repetition, while the E-Web unfolds, leaves 4.9 degrees down.

## 6. The vehicles

A vehicle fires from the muzzle bolts of its model (`WP_CalcVehMuzzle`), along each bolt's axis. Then (`FireVehicleWeapon`): a walker (the AT-ST) driven by a player always turns each shot towards the point a ray from its body along the view hits (`WP_VehCheckTraceFromCamPos`), a fighter does so only on a map larger than 20,000 units of culling distance; a weapon whose vehicle file sets `weap1Aim` or `weap2Aim` (the AT-ST's rocket, the X-wing's laser and missile) turns towards the point a ray along the vehicle's heading hits (`aimCorrect`, `WP_VehLeadCrosshairVeh`). The swoop sets neither: its lasers fire along its heading.

Measured (batch b): the swoop's lasers leave 23 forward and 43 below the rider's eyes, level along the swoop while the rider looks down at the defender 12.5 degrees, and hit its waist (40) and chest (16); the AT-ST's leave 95 forward, 3 or 4 to the side and 34 below, 7 to 10 degrees less steeply down than the view, towards the point the walker's ray hits (head 16, hand 8); the X-wing's leave its wing tips, 198 units to each side. The AT-ST and the X-wing hide their rider (`hideRider 1`), so their crosshair comes from the vehicle; the swoop's rider is drawn, so its crosshair is the rider's own, from its view: it does not follow the swoop's lasers (read from the code, not measured in cgame).

## 7. The NPCs and the bots

**The NPCs.** A stormtrooper turns to its enemy with `NPC_FaceEnemy`: from its eyes (the middle of its box at its eye height, `SPOT_HEAD_LEAN`) towards the enemy's eye point (`renderInfo.eyePoint`, which in multiplayer is the origin raised by the view height, `UpdateClientRenderinfo`), then fires through `FireWeapon` from the player's fixed point, along its view. Measured (batch b): every bolt leaves 12 forward, 6 right and 6 down from its eyes, along its view to the hundredth of a degree, and passes exactly 6 right and 6 below the bot's eye level. Against a bot standing with its saber, whose ready stance moves its head, the bolts pass 8 units to the side of the head and 2 above it: none of the 36 bolts touches it (225 in 48 repetitions in step 20 part 8). A bot holding the bryar is hit 16 times in 37, on its waist and hand.

No random aim error is added on that path: `NPC_UpdateAngles` turns at a limited speed only, and the aim value `currentAim` that `G_AimSet` sets is read in multiplayer by the Galak mech alone (`NPC_AI_GalakMech.cpp`). The trooper's view reached the eye level exactly at every shot. (Step 20 part 8 wrote that the trooper's aim error is large at its first sight and shrinks: wrong for the multiplayer game, corrected in [../progress/step-20-scenarios.md](../progress/step-20-scenarios.md) section 16.) The generic NPC attack check (`NPC_CheckAttack`) aims from the weapon's point (`SPOT_WEAPON`) instead, with its own wiggle; the trooper does not use it.

**The bots.** The bots' AI aims from its eyes (`bs->eye`, the origin raised by the view height, `ai_main.cpp`) at the enemy's origin or head level, with a random error from its accuracy and skill (`BotAimOffsetGoalAngles`), which is 0 against an enemy standing still ("he's not even moving"), then fires through `FireWeapon` from the fixed point: the same offset of 2 to 8 units right and 4 to 6 down. Not measured with the AI (the scenarios drive the bots' commands themselves); the "from the eyes" rows of section 3 are what a bot aiming exactly would hit. In step 20 part 4b, a rocket aimed from the eyes at the flag carrier passed beside its head.

## 8. The turrets, the sentry and the seeker

They have no view: each aims from its own gun. `misc_turretG2` turns towards the enemy's eye point, 5 units lower (15 when it hangs upside down), from its muzzle bolt, at 14 degrees of yaw and 3 of pitch per update, then fires along its barrel (`turretG2_aim`, `turretG2_head_think`); `misc_turret` (Hoth's) aims from its top at the enemy's eye point. The sentry fires from 24 above its origin towards the enemy's origin raised by 24 (`pas_fire`); measured, its bolts pass through the defender's chest (64 hits on the back). The seeker circles 20 units around a point 40 above its owner and fires at the enemy's origin, the middle of its box (`SeekerDroneUpdate`); measured, its bolts pass 24 units below the chest point and hit the waist and legs. These are aims, not offsets; nothing to compare with a view.

## 9. The saber

The blade hits where the model's blade is, from the animation; the view only chooses the attack. The staff's kicks trace from the hip to the foot, about 44 units ahead and 18 to the right (step 20 part 3). A thrown saber starts at the saber's bolt in the hand (`WP_SaberPositionUpdate`; measured: 11 forward, 7 right and 24 below the eyes) and flies along the view; from level 2 it is steered towards the point a ray from the eyes along the view hits (`saberFirstThrown`: the walls at level 2, the players too at level 3). Measured at levels 1 to 3 (batch b): the throw starts at the same point; it hits the defender 128 units away on the waist and the back, once on the head at level 3.

## 10. What other games do

Searched on 20 November 2026; primary sources (the games' own code or their engine's documentation) first.

| game or engine | where a shot starts | where it goes | source |
|---|---|---|---|
| Quake III Arena (1999) | 14 units in front of the eyes, on the view's line | along the view | `CalcMuzzlePoint`, `code/game/g_weapon.c`, id Software's source (GPL), https://github.com/id-Software/Quake-III-Arena |
| Half-Life 2 and the Source engine (Source SDK 2013) | bullets at the eyes (`Weapon_ShootPosition()` returns `EyePosition()`); the RPG's missile at the eyes plus 12 forward, 6 right and 3 down | bullets along the view; the missile along the view, parallel, as Jedi Academy; the bullet's tracer drawn from 16 forward, 2 right, 4 down (multiplayer: the weapon's attachment), for show | `src/game/shared/baseplayer_shared.cpp`, `src/game/server/hl2/weapon_rpg.cpp`, `src/game/shared/baseentity_shared.cpp` (`ComputeTracerStartPosition`), https://github.com/ValveSoftware/source-sdk-2013 |
| Lyra, Epic's sample game for Unreal Engine 5 (third person) | on the camera's line, moved forward to the point level with the weapon | along the camera's line towards its focus: the shot lands at the centre of the screen | `ULyraGameplayAbility_RangedWeapon::PerformLocalTargeting` passes `ELyraAbilityTargetingSource::CameraTowardsFocus`; `GetTargetingTransform`: "Move the start and focal point up in front of pawn"; copy of Epic's source read at https://git.singinwhale.com/singinwhale/RealtimeStyleTransferRuntime (`Source/LyraGame/Weapons/LyraGameplayAbility_RangedWeapon.cpp`) |
| Unreal Engine 5 documentation, "Implement a Projectile" (UE 5.9) | the launcher's `Muzzle` socket | towards the point a line trace from the camera hits (`FindLookAtRotation( SocketLocation, TargetPosition )`): the shots converge on the crosshair | https://dev.epicgames.com/documentation/unreal-engine/coder-08-implement-a-projectile-in-unreal-engine |
| VALORANT | not documented by Riot | "shots are going where you're aiming (ignoring spread or movement error)"; the muzzle flash and tracer are displayed on another frame than the shot's processing | "The State of Hit Registration", Kevin Lee, 6 August 2020, https://playvalorant.com/en-us/news/dev/the-state-of-hit-registration/ |
| Counter-Strike 2 | not documented by Valve; players' threads say the shot is traced from the camera and the tracer drawn from the gun for show (community, not verified) | | the release notes of 6 February 2024 added a setting to turn the first-person tracers off (https://liquipedia.net/counterstrike/2024-02-06_Patch, secondary); Source's base starts bullets at the eyes (above); CS:GO overrides `Weapon_ShootPosition` with an animation offset (a community plugin's notes, https://github.com/srcdslab/sm-plugin-fixfirebullets/pull/17, not verified) |

The two practices: the shot starts at the eyes or the camera and goes to the centre of the screen, the barrel only drawing the effect (Quake III, Source's bullets, Lyra, by the players' accounts CS2); or it starts at the barrel and is turned towards the point a ray from the camera hits, so that it converges on the crosshair (Unreal Engine 5's documentation). A projectile fired parallel to the view from a point beside it, as Jedi Academy and Half-Life 2's RPG do, lands beside the centre of the screen by that offset; Jedi Academy's dynamic crosshair moves to show it.

## 11. What is normal, what is not

Decided on 20 November 2026: the five fixed, as Unreal Engine 5's documentation does, in plan step 20 part 10 ([../PLAN.md](../PLAN.md) step 20; [../progress/step-20-scenarios.md](../progress/step-20-scenarios.md) section 18).

The base game's (all of it the same in 1.01): the offsets themselves, the crosshair, the NPCs' and the bots' aim, the E-Web's cannon, the vehicles' aim. For a player with the default crosshair, every shot but the emplaced gun's lands where the crosshair is drawn: the offset is visible as a crosshair that moves on the screen and as a shot that cannot pass a corner the eyes see past. The defects, to decide (none changed):

1. **The NPCs and the bots aim from their eyes and fire from the weapon's point.** Before: a stormtrooper 128 units away fires 36 bolts at a player standing with a saber and none touches it, each passing 6 units right and 6 below the eye level it aims at; a bot's rocket aimed at a head passes beside it. After (if the aim is taken from the weapon's point, as `NPC_CheckAttack` already does, or the shot turned towards the point aimed at): the bolts pass through the point aimed at (expected, not measured).
2. **The emplaced gun's crosshair.** Before: the crosshair on a target's chest 204 units away, every bolt passes 10 units beside it, and a thin model is missed. After (the crosshair drawn for the barrel that fires next, or the bolts turned towards the crosshair's point): the bolts land where the crosshair is.
3. **In third person the bolt does not leave the visible barrel.** Before: a blaster bolt appears about 6 units above and 6 behind the flash drawn at the gun's barrel, an old bryar's up to 24 behind and 12 above. After (the shot started at the barrel's bolt and turned towards the crosshair's point, as Unreal Engine 5's documentation does): the bolt leaves the flash; the start would then follow the animation, which the server would have to compute for every shot.
4. **`cg_dynamicCrosshair 0` and the E-Web.** Before: with the setting off, the crosshair stays at the centre of the screen while every projectile lands 2 to 8 units right and 4 to 6 below it, and the E-Web's shots 4.56 degrees above it. After (the setting removed, its default kept: [the rule of 12 November 2026](../progress/README.md)): the crosshair always shows where the shot lands. The setting is one of the archived cvars the plan's "as few settings as possible" would remove.
5. **The swoop's crosshair.** Before: the swoop's rider sees the crosshair along its own view while the lasers fire level along the swoop from 43 units lower (read from the code). After (the crosshair from the swoop's muzzles, as the AT-ST's and the X-wing's): it shows where the lasers go.

## 12. Method

The measuring code is not in the game: [../tools/gamestats/shot_offsets.patch](../tools/gamestats/shot_offsets.patch) adds, to `codemp/game`:

- the scenario options `aim=eyes` and `aim=muzzle` (the attacker's view on the defender's head or chest, from the eyes or from the weapon's point through `CalcMuzzlePoint`), `at=chest`, `nospread` (the random spread multiplied by 0, the random draws kept so that the rest of the game is the same) and `flash` (the weapon's world model put on the attacker's right hand as the player's model 4, its `*flash` bolt traced);
- a line `DBG shot` for every missile (`CreateMissile`), every disruptor and concussion beam, every stun baton and melee trace and every thrown saber: the start against the view of who fires (forward, right, up from the eyes), the direction's yaw and pitch against the view, and how far the shot's line passes from the target's head, chest and eye level, in the shot's right and up; a line `DBG eweb` with the E-Web's shot axis and the axis cgame's crosshair takes;
- the hits by location counted in `G_LocationBasedDamageModifier` and printed after the scenario's statistics (`DBG hit locations`).

[../tools/gamestats/shot_offsets.sh](../tools/gamestats/shot_offsets.sh) runs batches a (each weapon's head aimed at from the eyes and from the weapon's point, 30 servers) and b (the other shooters, the spread kept, the `*flash` bolts, 32 servers) with `gamestats_windows.sh`, `SEED=1`, `FAST=1`, `g_knockback 0` (the defender stays in place); its header gives the commands to apply the patch, build, run and restore. Two runs (20 November 2026) gave the same counts on all their 243 lines.
