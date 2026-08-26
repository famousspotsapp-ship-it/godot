# Steel Streets — architecture reference

Implementation-level reference for the bundled `steel_streets/` game project.
Every statement below is taken from the project files listed with it, so this
file is the source of truth when generated documentation (e.g. DeepWiki pages)
disagrees.

## What the project is

- A 160x144 Game Boy-style side-scrolling beat-'em-up, run by the engine built
  from this repository (`steel_streets/project.godot`).
- Pure GDScript: 15 scripts, ~1200 lines, no C#, no GDExtension, no custom
  engine modules. Nothing in `steel_streets/` requires engine changes.
- `project.godot` declares `config/features = ("4.3", "GL Compatibility")` and
  `renderer/rendering_method = "gl_compatibility"`, while `version.py` in this
  repo is `4.7-beta`. The project therefore opens in the locally built editor
  but is authored against the 4.3 feature set.

## Scene flow

`GameManager` (autoload, `scripts/game_manager.gd`) owns all scene changes via
`get_tree().change_scene_to_file()`; no other script calls it directly.

```
title_screen ──any input──▶ splash_screen ──2.5s or input──▶ level_1
                                                              │
                       boss_defeated (+1.5s) ──▶ victory ──any input──┐
                       lives exhausted        ──▶ game_over ──any input──▶ title
```

`title_screen.gd` calls `GameManager.reset_run()`, so lives/score/HP reset
only when the title screen loads. Player death with lives remaining reloads
`level_1` through `GameManager.goto_level()` (`scripts/player.gd:_die`).

## Run state and signals

`GameManager` holds `lives` (3), `score`, `max_hp`/`current_hp` (5) and emits
`score_changed`, `lives_changed`, `hp_changed`. `scripts/hud.gd` is the only
consumer; it rebuilds the heart row on every `hp_changed`. Boss health is *not*
routed through `GameManager` — `boss.gd` emits `boss_hp_changed` /
`boss_defeated`, and `level_1.gd` hands the boss to the HUD with
`call_deferred("_attach_boss_to_hud", boss)` because the HUD is instantiated
after the boss.

## Animation: no AnimationPlayer, no AnimationTree

All animation is `AnimatedSprite2D` + `SpriteFrames` built **at runtime** from
horizontal sprite strips by `scripts/anim_util.gd` (`AnimUtil.build(specs)`,
one `AtlasTexture` per frame). Each entity declares a `FRAMES_SPEC` constant
(name, path, frame count, w/h, fps, loop) and calls
`sprite.sprite_frames = AnimUtil.build(FRAMES_SPEC)` in `_ready()`.

Consequences when editing the project:

- There are no `AnimationPlayer`, `AnimationTree`, `AnimationNodeStateMachine`,
  method tracks, or `.anim`/`.res` animation resources anywhere in the project.
- Attack timing is driven by timers in `_physics_process`, not animation keys:
  `player.gd` enables `AttackHitbox` only while
  `attack_timer > 0 and attack_timer < ATTACK_TIME * 0.75`.
- Hit flash and invincibility are code-driven, not shader-driven:
  `enemy_base.gd` strobes `sprite.modulate` between `Color(2,2,2,1)` and white
  for 0.18 s; `player.gd` toggles `sprite.visible` at 12 Hz for
  `INVINCIBLE_TIME = 1.2` s. No `ShaderMaterial` is used.
- There are no particles (`CPUParticles2D`/`GPUParticles2D`) in the project.

## Combat wiring

| Concern | Implementation |
| --- | --- |
| Player damage output | `AttackHitbox` (Area2D) in group `player_attack`; enemies read `get_attack_damage()` off its parent (`enemy_base.gd:_on_hurtbox_area_entered`) |
| Enemy damage output | `Hitbox` Area2D `body_entered` → `player.take_damage(contact_damage, global_position)` |
| Enemy death | `enemy_base.gd:_die()` adds score, defers `monitoring`/`monitorable` off, `queue_free()` |
| Player death | `player.gd:_die()` → `GameManager.take_life()` → respawn level or game over |
| Group lookups | `get_tree().get_first_node_in_group("player")`, groups: `player`, `player_attack`, `enemy`, `boss`, `ladder` |

Enemy behaviours are hand-rolled state logic, all in `_physics_process`:

- `foot_soldier.gd`: patrol 22 px/s, chase 35 px/s within `CHASE_RANGE = 70`,
  turns at walls and at ledges via a downward `intersect_ray` probe.
- `shuriken_thrower.gd`: paces ±14 px around spawn, fires
  `scenes/projectile.tscn` every 1.6 s within `SIGHT_RANGE = 110`.
- `boss.gd`: 9 HP, explicit `enum State { PAUSE, WINDUP, CHARGE, RECOVER }`
  cycle with a `match` statement — not an `AnimationTree` state machine.
- `projectile.gd`: 110 px/s straight line, 3 s lifetime, frees on player hit or
  `TileMap`/`StaticBody2D` contact.

## Physics layers

Defined in `project.godot` `[layer_names]` (8 layers, not 4):

| Bit | Layer | Used by |
| --- | --- | --- |
| 1 | `world` | TileSet physics layer, set in `level_1.gd:_build_tileset()` |
| 2 | `player` | `Player` body (`collision_layer = 2`, mask `1`) |
| 3 | `player_hitbox` | `Player/AttackHitbox` (layer `4`, mask `72` = enemy + pickup) |
| 4 | `enemy` | Enemy bodies and their `Hurtbox` (layer `8`, hurtbox mask `4`) |
| 5 | `enemy_hitbox` | Enemy `Hitbox` and `Projectile` (layer `16`) |
| 6 | `ladder` | Ladder areas created in code (`collision_layer = 32`) |
| 7 | `pickup` | `QuestionBlock` (layer `64`, mask `6` = player + player hitbox) |
| 8 | `hazard` | Spike areas created in code (`collision_layer = 128`) |

Ladder handling is a counter, not a state flag: ladder areas call
`enter_ladder()` / `exit_ladder()` on the player, which tracks
`ladders_overlapping` and drops out of `climbing` when it reaches zero.

## Level 1 is generated in code

`scripts/level_1.gd` builds everything at `_ready()` instead of authoring a
large `.tscn`:

- `_build_tileset()` creates a `TileSet` + `TileSetAtlasSource` from
  `assets/sprites/tiles/tileset.png`, adds the source **before** creating the
  11 atlas tiles (so tiles inherit the physics layer), and gives `A_BRICK`,
  `A_CONCRETE`, `A_ROOF` full-tile collision polygons.
- World is 100x9 tiles of 16 px, ground at row 8; background (layer 0) and
  foreground/solid platforms (layer 1) are painted from the `PLATFORMS`,
  `LADDERS`, `SPIKES`, `PICKUPS`, `FOOT_SOLDIERS`, `SHURIKEN_THROWERS`,
  `BOSS_POS` constants at the top of the file — tweak the level there.
- The node is a `TileMap` with two layers (`add_layer`), i.e. the legacy
  node, not the newer `TileMapLayer` nodes.
- The `Camera2D` is created in code and parented to the player, with smoothing
  speed 8 and limits clamped to the world bounds.

## Assets are reproducible

`tools/gen_assets.py` (Pillow + `wave`) regenerates every PNG and WAV under
`assets/` in the 4-colour Game Boy palette. Output paths are resolved relative
to the script, so it can be run from any directory:

```bash
python3 steel_streets/tools/gen_assets.py
```

## Running the project

Use the editor/binary built from this repo (see the repo `README.md` and
`SConstruct` for build flags):

```bash
scons target=editor dev_build=yes -j$(nproc)          # build the editor
./bin/godot.linuxbsd.editor.dev.x86_64 --path steel_streets   # run the game
```

Any Godot 4.3+ binary also runs the project unchanged (`godot --path
steel_streets`).
