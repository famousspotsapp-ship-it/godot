# Steel Streets — architecture

Implementation reference for the bundled `steel_streets/` game project. Every
statement here is taken from the files in this directory; see
[README.md](README.md) for how to run and play the game.

## Engine and project settings

- The project is run by the engine built from this repository (currently
  `4.7-beta`, see `version.py`), while `project.godot` still declares
  `config/features=PackedStringArray("4.3", "GL Compatibility")`. Anything
  4.3-compatible is therefore safe; newer APIs are available but will make the
  project unusable on the 4.3 editor the features tag advertises.
- Renderer: `gl_compatibility` (desktop and mobile).
- Viewport is 160x144 with `stretch/mode="viewport"`, `aspect="keep"`, a 640x576
  window override, nearest-neighbour texture filtering, and 2D transform/vertex
  pixel snapping.
- Single autoload: `GameManager` -> `scripts/game_manager.gd`.
- Eight named 2D physics layers: `world`, `player`, `player_hitbox`, `enemy`,
  `enemy_hitbox`, `ladder`, `pickup`, `hazard`.
- Input actions (keyboard + gamepad): `move_left`, `move_right`, `jump`,
  `attack`, `climb_up`, `climb_down`, `start`.

## GameManager (autoload)

Owns run state across scene changes and is the only place that changes scenes.

State: `lives` (starts at 3), `score`, `max_hp`/`current_hp` (start at 5).

Signals: `score_changed(new_score)`, `lives_changed(new_lives)`,
`hp_changed(current, maximum)`.

API: `reset_run()`, `add_score(amount)`, `set_hp(value)`, `heal(amount)`,
`take_life()` (returns `false` when out of lives, otherwise refills HP), and the
navigation helpers `goto_title()`, `goto_splash()`, `goto_level()`,
`goto_game_over()`, `goto_victory()`, each a `change_scene_to_file()` call.

## Scene flow

`title_screen` (calls `reset_run()`, blinks "PRESS START", any key/pad/mouse
press advances) -> `splash_screen` (auto-advances after 2.5 s or on input) ->
`level_1` -> `victory` (boss defeated) or `game_over` (out of lives). Both
terminal screens return to the title.

## Level construction

`scripts/level_1.gd` builds Level 1 entirely in code — no hand-authored tilemap
`.tscn`. It generates the `TileSet` from `assets/sprites/tiles/tileset.png`
(16 px tiles, atlas coords `A_BRICK`, `A_CONCRETE`, `A_ROOF`, `A_BRICK_BG`,
`A_WINDOW`, `A_DOOR`, two sky patterns), paints background and foreground
layers, then instantiates ladders, spikes, question blocks, enemies, the boss,
the player, the HUD, and the camera from the const tables at the top of the file
(`PLATFORMS`, `LADDERS`, `SPIKES`, `PICKUPS`, `FOOT_SOLDIERS`,
`SHURIKEN_THROWERS`). World is 100x9 tiles with the ground on row 8. Level
layout changes are edits to those tables, not to a scene file.

## Animation

There is no `AnimationPlayer`, `AnimationTree`, or `ShaderMaterial` anywhere in
the project. Every animated entity is an `AnimatedSprite2D` whose
`sprite_frames` is built at runtime in `_ready()` by
`scripts/anim_util.gd::AnimUtil.build(specs)`, which slices horizontal sprite
strips into `AtlasTexture` frames. Each script declares its own `FRAMES_SPEC`
array of `{name, path, count, w, h, fps, loop}` dictionaries.

## Player (`scripts/player.gd`, `CharacterBody2D`)

- Speed 60, jump velocity -190, gravity 700, climb speed 50.
- Attack: `_start_attack()` plays the non-looping `attack` animation and enables
  the `AttackHitbox` `Area2D` (in group `player_attack`) for `ATTACK_TIME`
  (0.30 s) with a 0.15 s cooldown; the hitbox is offset 10 px in the facing
  direction. Damage is 1, exposed to enemies via `get_attack_damage()`.
- Ladders: `enter_ladder()` / `exit_ladder()` maintain an overlap counter;
  climbing zeroes gravity and uses `climb_up`/`climb_down`.
- Damage: `take_damage(amount, from_position)` is ignored while
  `invincible_timer > 0`; a hit applies knockback (80, -120) and 1.2 s of
  invincibility, visualised by toggling `sprite.visible` (no shader flash).

## Enemies

`scripts/enemy_base.gd` (`CharacterBody2D`) is the shared base — subclasses use
`extends "res://scripts/enemy_base.gd"` and call `super()` in `_ready()`. It
owns `max_hp`, `contact_damage`, `score_value`, the optional `Sprite`,
`Hurtbox`, `Hitbox`, and `SfxHit` children, adds the node to group `enemy`, and
wires `Hurtbox.area_entered` (accepts areas in group `player_attack`, reading
damage from the attacker's `get_attack_damage()`) plus `Hitbox.body_entered`
(deals `contact_damage` to the player). `take_damage()` sets an 0.18 s
`hit_flash_timer` that strobes `sprite.modulate`; `_die()` awards
`score_value` through `GameManager.add_score()`, deferred-disables both areas,
and calls `queue_free()`.

| Enemy | HP | Score | Behaviour |
| --- | --- | --- | --- |
| `foot_soldier.gd` | 2 | 100 | Patrols at 22 px/s, chases at 35 px/s within 70 px |
| `shuriken_thrower.gd` | 3 | 150 | Paces +-14 px, fires `projectile.tscn` every 1.6 s within 110 px |
| `boss.gd` | 9 | 1000 | `State` enum cycle PAUSE -> WINDUP -> CHARGE -> RECOVER, charge speed 70; emits `boss_hp_changed` / `boss_defeated` |

The boss state machine is plain GDScript (`match state` on the `State` enum) —
not an `AnimationNodeStateMachine`.

`scripts/projectile.gd` is an `Area2D` that moves 110 px/s along `direction`,
deals 1 damage, and frees itself after a 3 s lifetime or on hitting a `TileMap`
or `StaticBody2D`.

## HUD and pickups

`scripts/hud.gd` (`CanvasLayer`) connects to the three `GameManager` signals,
rebuilds the heart row from `heart.png` / `heart_empty.png`, formats score as
`%05d`, and exposes `attach_boss(boss)` so `level_1` can show the boss bar
driven by `boss_hp_changed` / `boss_defeated`.

`scripts/question_block.gd` is an `Area2D` consumed either by the player body or
by a `player_attack` area; it awards 50 score, heals 1, plays the pickup SFX,
and frees itself.

## Assets

All PNGs and WAVs are generated by `tools/gen_assets.py` (Pillow + `wave`); edit
the generator rather than the binaries, then re-run
`python3 tools/gen_assets.py` from this directory.
