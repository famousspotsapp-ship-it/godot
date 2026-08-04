# Steel Streets — architecture

Technical reference for the bundled game project. For install/controls, see
[README.md](README.md). Paths in this document are relative to
`steel_streets/`; `res://` maps to this directory.

Steel Streets is a plain Godot 4 project living at the root of the engine fork.
It is **not** an engine module and is not compiled into the binary — nothing in
`core/`, `scene/`, `servers/`, or `modules/` was changed for it. Run it with any
Godot 4.3+ editor, or with a binary built from this tree:

```bash
./bin/godot.linuxbsd.editor.dev.x86_64 --path steel_streets
```

## Project configuration (`project.godot`)

| Setting | Value | Why |
| :--- | :--- | :--- |
| `display/window/size/viewport_{width,height}` | `160 x 144` | Game Boy resolution; the game logic works entirely in these units. |
| `display/window/size/window_{width,height}_override` | `640 x 576` | 4x integer scale for a usable desktop window. |
| `display/window/stretch/mode` | `viewport` | The whole frame is rendered at 160x144 and scaled up by the root viewport. There is no `SubViewport` node in any scene. |
| `display/window/stretch/aspect` | `keep` | Preserves the 10:9 aspect with letterboxing. |
| `rendering/textures/canvas_textures/default_texture_filter` | `0` (nearest) | Keeps pixels crisp when upscaled. |
| `rendering/renderer/rendering_method` | `gl_compatibility` | Runs on the OpenGL/GLES3 backend, incl. mobile and web. |
| `rendering/2d/snap/*` | `true` | Snaps 2D transforms and vertices to whole pixels. |
| `autoload/GameManager` | `res://scripts/game_manager.gd` | Single global singleton (see below). |
| `application/config/features` | `"4.3"`, `"GL Compatibility"` | Project was authored against 4.3; the engine in this fork is 4.7-beta. |

The four-colour Game Boy palette (`#e0f8d0`, `#88c070`, `#346856`, `#081820`)
is **baked into the generated PNGs** — there is no palette-swap shader and no
`ShaderMaterial` anywhere in the project.

## Runtime structure

```
GameManager (autoload, Node)
└── current scene (swapped via SceneTree.change_scene_to_file)
    title_screen → splash_screen → level_1 → victory | game_over → title_screen
```

`GameManager` (`scripts/game_manager.gd`) owns everything that must survive a
scene change and is the only place scene transitions happen:

- State: `lives` (3), `score`, `max_hp`/`current_hp` (5).
- Signals: `score_changed`, `lives_changed`, `hp_changed` — the HUD is a pure
  subscriber and never polls.
- Navigation helpers: `goto_title/splash/level/game_over/victory`, plus
  `reset_run()` (called by the title screen) and `take_life()`, which returns
  `false` once lives run out.

Player death therefore reloads `level_1.tscn` from scratch; there is no
checkpoint or persistence of level state.

## Scenes

| Scene | Root type | Notes |
| :--- | :--- | :--- |
| `title_screen.tscn` | `Control` | Blinks "PRESS START", advances on any key/pad/mouse press. Calls `GameManager.reset_run()` on ready. |
| `splash_screen.tscn` | `Control` | Auto-advances after 2.5 s or on input. |
| `level_1.tscn` | `Node2D` | Nearly empty on disk: a `TileMap`, an `Entities` node, and a BGM player. All content is built in code (below). |
| `hud.tscn` | `CanvasLayer` | Hearts, lives, score, and a boss bar that is hidden until `attach_boss()` is called. |
| `player.tscn` | `CharacterBody2D` | `Sprite` (`AnimatedSprite2D`), body `CollisionShape2D`, `AttackHitbox` (`Area2D`), three `AudioStreamPlayer`s. |
| `foot_soldier.tscn`, `shuriken_thrower.tscn`, `boss.tscn` | `CharacterBody2D` | Each has `Sprite`, `Hurtbox` (takes hits), `Hitbox` (deals contact damage), `SfxHit`. |
| `projectile.tscn` | `Area2D` | Enemy shuriken. |
| `question_block.tscn` | `Area2D` | Score + heal pickup. |
| `game_over.tscn`, `victory.tscn` | `Control` | Return to the title on input. |

## Animation

There is **no `AnimationPlayer`, `AnimationTree`, or `AnimationNodeStateMachine`
in this project.** All animation is frame-based `AnimatedSprite2D` playback, and
the `SpriteFrames` resources are built at runtime rather than stored as
resources:

`scripts/anim_util.gd` exposes `AnimUtil.build(specs) -> SpriteFrames`. Each
spec entry is `{name, path, count, w, h, fps, loop}`; the helper loads the
horizontal strip PNG and slices it into `count` `AtlasTexture` frames of
`w x h`. Every animated entity declares a `FRAMES_SPEC` constant and calls
`AnimUtil.build()` in `_ready()`.

Animation *state* is plain GDScript: `Player._update_animation()` picks
`attack` / `climb` / `jump` / `walk` / `idle` by inspecting timers and
`is_on_floor()`. Hitbox activation is likewise manual — `Player`
`_update_attack_state()` enables `AttackHitbox.monitoring` only during the
middle of the 0.30 s swing window, rather than keying it from an animation
track.

## Level construction

`scripts/level_1.gd` (the largest script, ~300 lines) builds the whole level
procedurally in `_ready()`, so the `.tscn` stays tiny and diffable:

1. `_build_tileset()` creates a `TileSet` + `TileSetAtlasSource` from
   `assets/sprites/tiles/tileset.png` (11 tiles in a 1-row atlas, 16x16), adds
   physics layer 0 on collision layer `world`, and gives the three solid tiles
   (`brick`, `concrete`, `roof`) a full-tile collision polygon.
2. `_paint_background()` / `_paint_foreground()` fill `TileMap` layer 0 (sky,
   brick wall, windows, doors, clouds) and layer 1 (ground and rooftops) from
   the `PLATFORMS` table. The world is 100 x 9 tiles.
3. `_spawn_decor()` builds ladders and spikes entirely from code
   (`Area2D` + `Sprite2D` + `RectangleShape2D`). Ladders call
   `enter_ladder()` / `exit_ladder()` on the player via duck typing; spikes deal
   1 damage on body entry.
4. `_spawn_pickups/_spawn_enemies/_spawn_player/_spawn_boss/_spawn_hud`
   instantiate the packed scenes at the tile coordinates listed in the constant
   tables at the top of the file. The `Camera2D` is created in code, parented to
   the player, smoothed, and limited to the world bounds.
5. The boss is wired up with `call_deferred("_attach_boss_to_hud", boss)` so the
   HUD exists first; `boss_defeated` waits 1.5 s and then goes to victory.

Editing the level means editing the constant tables (`PLATFORMS`, `LADDERS`,
`SPIKES`, `PICKUPS`, `FOOT_SOLDIERS`, `SHURIKEN_THROWERS`, `BOSS_POS`,
`PLAYER_SPAWN`), not the scene file.

`TileMap` is deprecated upstream in favour of `TileMapLayer`; the project still
uses `TileMap` with two layers. It works in 4.3+ but will warn in the editor.

## Collision layers

Layer names are defined in `project.godot` under `[layer_names]`.

| Bit | Layer | Used by |
| :--- | :--- | :--- |
| 1 | `world` | Tileset physics layer. |
| 2 | `player` | Player body. |
| 3 | `player_hitbox` | Player `AttackHitbox`. |
| 4 | `enemy` | Enemy bodies and `Hurtbox`es. |
| 5 | `enemy_hitbox` | Enemy `Hitbox`es and shurikens. |
| 6 | `ladder` | Ladder areas (built in code). |
| 7 | `pickup` | Question blocks. |
| 8 | `hazard` | Spikes (built in code). |

Resulting masks: player body `1`; player attack hitbox `8|64` (enemies +
pickups); enemy hurtbox `4` (player hitbox); enemy hitbox `2` (player);
projectile `1|2`; question block `2|4`; ladder/spike `2`.

## Combat model

`scripts/enemy_base.gd` is the shared base (`foot_soldier.gd`,
`shuriken_thrower.gd`, and `boss.gd` all `extends "res://scripts/enemy_base.gd"`
and override `max_hp` / `contact_damage` / `score_value` before calling
`super()`). It owns HP, the white hit-flash, contact damage, and the death
handshake (award score → deferred-disable both areas → `queue_free()`).

Cross-entity communication is group- and duck-typing-based rather than typed
references: groups `player`, `player_attack`, `enemy`, `boss`, `ladder`, and
`has_method()` checks for `take_damage`, `get_attack_damage`, `enter_ladder`,
`exit_ladder`, `attach_boss`.

| Entity | HP | Score | Behaviour |
| :--- | :--- | :--- | :--- |
| Player | 5 HP x 3 lives | — | 60 px/s walk, -190 jump, 700 gravity, 1.2 s i-frames with a 12 Hz flash, knockback on hit. |
| Foot soldier | 2 | 100 | Patrols, turns at walls and ledges (a downward `PhysicsRayQueryParameters2D` probe against layer 1), chases within 70 px. |
| Shuriken thrower | 3 | 150 | Paces ±14 px around spawn, fires a shuriken every 1.6 s when the player is within 110 px. |
| Boss | 9 | 1000 | `PAUSE → WINDUP → CHARGE → RECOVER` state machine (a GDScript `enum`, not an animation state machine), emits `boss_hp_changed` / `boss_defeated` for the HUD. |

## Asset pipeline

Every PNG and WAV under `assets/` is generated by
`tools/gen_assets.py` (Pillow + the stdlib `wave` module) — sprites are written
as pixel-row string art constrained to the four palette entries, and the audio
is synthesised square/noise chiptune. Regenerate from the repo root:

```bash
python3 steel_streets/tools/gen_assets.py   # requires: pip install Pillow
```

Do not hand-edit the generated files; change `gen_assets.py` and re-run it, then
commit both the script and the regenerated binaries (Godot `.import` files are
committed alongside them).

## Conventions when extending the game

- Keep everything under `steel_streets/`; no `res://` path may leave the project.
- New animated entity: add a `FRAMES_SPEC` constant + `AnimUtil.build()` in
  `_ready()`, and add the sprite strip to `gen_assets.py`.
- New enemy: extend `enemy_base.gd`, set the three exported stats before
  `super()`, and reuse the `Hurtbox`/`Hitbox` layer setup from an existing
  enemy scene.
- New level content: prefer adding rows to the constant tables in `level_1.gd`.
- Run state belongs in `GameManager`; per-level state belongs in `level_1.gd`.
