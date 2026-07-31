# Steel Streets — architecture reference

Implementation notes for the bundled demo game. `README.md` covers how to run
and play it; this file describes how it is actually built, so tooling and new
contributors do not have to infer the design from the scene files.

## Where things live

| Path | Contents |
| :--- | :--- |
| `project.godot` | Input map, physics layer names, viewport/stretch config, `GameManager` autoload |
| `scenes/` | Screens (`title_screen`, `splash_screen`, `level_1`, `game_over`, `victory`) and entities (`player`, `foot_soldier`, `shuriken_thrower`, `boss`, `projectile`, `question_block`, `hud`) |
| `scripts/` | One GDScript per scene plus `game_manager.gd`, `enemy_base.gd`, `anim_util.gd` (~1.2k lines total) |
| `assets/sprites`, `assets/audio` | Generated PNG strips and WAVs — do not hand-edit |
| `tools/gen_assets.py` | Regenerates every sprite and sound (Pillow + `wave`) |

Note the project lives at the repository root as `steel_streets/`, not under a
`projects/` directory.

## Rendering and the Game Boy look

The retro presentation comes from project settings, not from shaders or a
`SubViewport`:

- `display/window/size/viewport_{width,height} = 160x144` with
  `window/stretch/mode="viewport"` and `aspect="keep"`; the window override is
  640x576.
- `rendering/textures/canvas_textures/default_texture_filter=0` (nearest) plus
  2D transform/vertex pixel snapping.
- `renderer/rendering_method="gl_compatibility"` on both desktop and mobile.
- The four-color palette (`#e0f8d0`, `#88c070`, `#346856`, `#081820`) is baked
  into the generated PNGs by `tools/gen_assets.py`. There is no palette shader
  and no `Theme`/`ThemeDB` customization; UI text uses a custom 5x7 bitmap font.

## Animation

There is no `AnimationPlayer`, `AnimationTree`, or `AnimationNodeStateMachine`
anywhere in the project. Every animated entity is an `AnimatedSprite2D` whose
`SpriteFrames` resource is built **at runtime**: each script declares a
`FRAMES_SPEC` array (name, sheet path, frame count, frame size, fps, loop) and
passes it to `AnimUtil.build()` (`scripts/anim_util.gd`), which slices the
horizontal sprite sheet into `AtlasTexture` frames.

Animation state is plain GDScript: `player.gd` picks a clip in
`_update_animation()` based on `climbing` / `attack_timer` / `is_on_floor()`,
and hitboxes are toggled imperatively (`Area2D.monitoring`,
`CollisionShape2D.disabled`) rather than by animation tracks.

## Gameplay structure

- `GameManager` (autoload, `scripts/game_manager.gd`) owns lives (3), score,
  and HP (5) across scenes, exposes `score_changed` / `lives_changed` /
  `hp_changed` signals, and centralizes every scene transition.
- `player.gd` is a `CharacterBody2D` with hand-rolled gravity (`700`), speed
  (`60`), jump velocity (`-190`), a `0.30 s` attack window, and `1.2 s` of
  invincibility with a strobe flash.
- `enemy_base.gd` is the shared enemy base (HP, hurtbox/hitbox wiring, hit
  flash, score drop, `queue_free`); `foot_soldier.gd` patrols and chases,
  `shuriken_thrower.gd` paces and lobs `projectile.tscn`, and `boss.gd` cycles
  pause → wind-up → charge → recover and emits `boss_hp_changed` /
  `boss_defeated`.
- `hud.tscn`/`hud.gd` is a `CanvasLayer` that rebuilds heart `TextureRect`s from
  `GameManager` signals and shows a boss bar via `attach_boss()`.
- Combat uses named 2D physics layers declared in `project.godot`: `world`,
  `player`, `player_hitbox`, `enemy`, `enemy_hitbox`, `ladder`, `pickup`,
  `hazard`.

## Level construction

`level_1.tscn` contains almost no level data. `scripts/level_1.gd` builds the
whole stage in `_ready()`: it constructs the `TileSet` from
`assets/sprites/tiles/tileset.png`, adds a second tile layer for solid
foreground tiles, paints background/foreground rectangles from the `PLATFORMS`
table, then spawns ladders, spikes, question blocks, enemies, the boss, the
player, the camera, and the HUD from constant tables (`LADDERS`, `SPIKES`,
`PICKUPS`, `FOOT_SOLDIERS`, `SHURIKEN_THROWERS`, `BOSS_POS`). The world is
100x9 tiles of 16 px (~6 screens wide).

Tuning the level means editing those constants, not the `.tscn`.

## Known drift against the engine in this repo

`project.godot` declares `config/features=PackedStringArray("4.3", "GL Compatibility")`
and `level_1.gd` uses the `TileMap` node, which is deprecated in this tree
(`doc/classes/TileMap.xml` points at `TileMapLayer`). The demo still runs, but
opening it with the 4.7-beta editor built from this repository will report the
deprecation. Porting `level_1.gd` to two `TileMapLayer` nodes (and updating the
`body is TileMap` check in `projectile.gd`) is the follow-up.
