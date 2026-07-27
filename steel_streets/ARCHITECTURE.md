# Steel Streets — Architecture

Reference for how the demo is actually wired, intended as the source of truth for
tooling and generated documentation.

## Runtime shape

- Engine features: `4.3`, renderer `gl_compatibility` (see `project.godot`).
- Native resolution is 160x144 with `window/stretch/mode="viewport"` and
  `aspect="keep"`; the window opens at 640x576. There is **no** `SubViewport`
  and **no** palette shader — the four-colour Game Boy palette is baked into the
  generated PNGs by `tools/gen_assets.py`.
- Pixel snapping (`snap_2d_transforms_to_pixel`, `snap_2d_vertices_to_pixel`) and
  `default_texture_filter=0` (nearest) keep the art crisp.
- Single autoload: `GameManager` (`scripts/game_manager.gd`).

## Screen flow

`GameManager` owns run state (`score`, `lives`, `current_hp`, `max_hp`), emits
`score_changed` / `lives_changed` / `hp_changed`, and performs every scene
change via `get_tree().change_scene_to_file()`:

```
title_screen.tscn --any input--> splash_screen.tscn --2.5s or input--> level_1.tscn
level_1 --boss_defeated--> victory.tscn
level_1 --lives exhausted--> game_over.tscn
victory / game_over --any input--> title_screen.tscn
```

Run constants: `STARTING_LIVES = 3`, `STARTING_HP = 5`.

## Animation

Animations do **not** use `AnimationPlayer`, `AnimationTree`, or
`AnimationNodeStateMachine`. Every animated entity is an `AnimatedSprite2D`
whose `SpriteFrames` is built at `_ready()` by `AnimUtil.build()`
(`scripts/anim_util.gd`) from a `FRAMES_SPEC` array: each entry names a
horizontal strip PNG plus frame `count`, `w`, `h`, `fps`, and `loop`, and is
sliced into `AtlasTexture` regions. Animation selection is plain GDScript
branching (e.g. `Player._update_animation()`).

## Level construction

`level_1.tscn` is nearly empty; `scripts/level_1.gd` builds the level in code:

- `_build_tileset()` creates a `TileSet` + `TileSetAtlasSource` from
  `assets/sprites/tiles/tileset.png` (11 tiles, 16px), adds physics layer 0 on
  collision layer 1, and gives `A_BRICK` / `A_CONCRETE` / `A_ROOF` a full-tile
  collision polygon.
- The node is a `TileMap` (deprecated in favour of `TileMapLayer` in Godot 4.3+)
  with layer 0 as background and layer 1 as foreground/solid, painted from the
  `PLATFORMS` table over a 100x9 tile world.
- Ladders, spikes, pickups, enemies, the boss, the player, the HUD, and a
  `Camera2D` (smoothed, clamped to world bounds, parented to the player) are all
  instantiated in `_ready()`. Ladders and spikes are `Area2D`s assembled at
  runtime rather than scenes.

## Combat and collision

Physics layers (`project.godot` `[layer_names]`): 1 world, 2 player,
3 player_hitbox, 4 enemy, 5 enemy_hitbox, 6 ladder, 7 pickup, 8 hazard.
Node groups carry most of the gameplay coupling: `player`, `player_attack`,
`enemy`, `boss`, `ladder`.

- Player (`CharacterBody2D`): `SPEED 60`, `JUMP_VELOCITY -190`, `GRAVITY 700`,
  `CLIMB_SPEED 50`, `ATTACK_TIME 0.30` + `0.15` cooldown, `INVINCIBLE_TIME 1.2`
  (visibility strobed at 12 Hz). The attack `Area2D` is offset `±10px` and only
  monitors during the middle of the swing window.
- Ladder climbing is refcounted through `enter_ladder()` / `exit_ladder()` calls
  made by the ladder `Area2D`.
- `enemy_base.gd` owns hp, hit-flash, contact damage, score payout, and death;
  `foot_soldier.gd` (patrol + chase, ledge raycast), `shuriken_thrower.gd`
  (paces, fires `projectile.tscn` every 1.6 s within 110px), and `boss.gd`
  (`PAUSE → WINDUP → CHARGE → RECOVER` enum state machine, 9 hp, 1000 points)
  extend it via `extends "res://scripts/enemy_base.gd"`.
- `question_block.gd` (`Area2D`) awards 50 points and heals 1 on touch or attack.

## HUD

`hud.tscn` is a `CanvasLayer` added by the level. It reacts to the
`GameManager` signals to redraw hearts, lives, and a zero-padded score, and
exposes `attach_boss()` so the level can wire the boss health bar after
`_ready()` (called via `call_deferred`).

## Assets

All sprites and audio are regenerated deterministically:

```bash
cd steel_streets && python3 tools/gen_assets.py   # requires Pillow
```

## Running

```bash
godot --path steel_streets
# or, with a locally built editor binary from this repo:
./bin/godot.linuxbsd.editor.dev.x86_64 --path steel_streets
```
