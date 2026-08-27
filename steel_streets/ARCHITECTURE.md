# Steel Streets — Architecture

Reference for the bundled `steel_streets/` game project. Everything below is
taken from the code in this directory; use it as the source of truth when the
generated codebase wiki and the code disagree.

## Engine version

The project's `config/features` tag is `4.3`, so the minimum supported editor is
Godot 4.3. It also runs on the engine built from this repository, which is
currently **4.7-beta** (see `version.py` at the repo root).

## Renderer and viewport

`project.godot` pins the retro presentation:

| Setting | Value |
| ------- | ----- |
| `renderer/rendering_method` (+ `.mobile`) | `gl_compatibility` |
| `window/size/viewport_{width,height}` | `160` x `144` |
| `window/size/window_{width,height}_override` | `640` x `576` |
| `window/stretch/mode` / `aspect` | `viewport` / `keep` |
| `textures/canvas_textures/default_texture_filter` | `0` (nearest) |
| `2d/snap/snap_2d_{transforms,vertices}_to_pixel` | `true` |

## 2D physics layers

Layer names are declared under `[layer_names]` in `project.godot`. Bodies and
areas set raw bitmasks in the `.tscn` files, so keep this table in sync when
adding entities.

| Layer | Bit value | Name | Used by |
| ----- | --------- | ---- | ------- |
| 1 | 1 | `world` | tileset physics layer (`level_1.gd`) |
| 2 | 2 | `player` | `player.tscn` body |
| 3 | 4 | `player_hitbox` | `AttackHitbox` on `player.tscn` |
| 4 | 8 | `enemy` | enemy bodies and their `Hurtbox` |
| 5 | 16 | `enemy_hitbox` | enemy `Hitbox`, `projectile.tscn` |
| 6 | 32 | `ladder` | ladder areas built in `level_1.gd` |
| 7 | 64 | `pickup` | `question_block.tscn` |
| 8 | 128 | `hazard` | spike areas built in `level_1.gd` |

## Scene flow

`GameManager` (`scripts/game_manager.gd`) is the only autoload. It owns the
persistent run state (`lives` = 3, `max_hp` = 5, `score`) and every scene
transition through `goto_title/goto_splash/goto_level/goto_game_over/goto_victory`,
each of which calls `SceneTree.change_scene_to_file()`.

```
title_screen ──any input──> splash_screen ──2.5 s or input──> level_1
level_1 ──boss_defeated (1.5 s)──> victory ──any input──> title_screen
level_1 ──player death, lives left──> level_1 (reload)
level_1 ──player death, no lives──> game_over ──any input──> title_screen
```

It exposes three signals consumed by the HUD: `score_changed(new_score)`,
`lives_changed(new_lives)`, `hp_changed(current, maximum)`. `take_life()`
returns `false` once lives run out, which is what sends the player to the
game-over screen.

## Node hierarchy of the game scripts

| Script | Extends | Notes |
| ------ | ------- | ----- |
| `game_manager.gd` | `Node` | autoload `GameManager` |
| `player.gd` | `CharacterBody2D` | group `player`, hitbox in group `player_attack` |
| `enemy_base.gd` | `CharacterBody2D` | group `enemy`, shared HP/contact damage/death |
| `foot_soldier.gd` | `enemy_base.gd` | patrol + chase melee grunt |
| `shuriken_thrower.gd` | `enemy_base.gd` | paces, fires `projectile.tscn` |
| `boss.gd` | `enemy_base.gd` | group `boss`, 4-state charge cycle |
| `projectile.gd` | `Area2D` | straight-line shuriken (not a `Node2D`) |
| `question_block.gd` | `Area2D` | score + heal pickup |
| `hud.gd` | `CanvasLayer` | hearts, lives, score, boss bar |
| `level_1.gd` | `Node2D` | builds the whole level in code |
| `anim_util.gd` | `Object` | static `build()` helper |
| `title_screen.gd`, `splash_screen.gd`, `game_over.gd`, `victory.gd` | `Control` | menu screens |

Enemy subclasses inherit by path (`extends "res://scripts/enemy_base.gd"`), set
`max_hp` / `contact_damage` / `score_value` before calling `super()` in
`_ready()`, and then build their own `SpriteFrames`.

## Combat

* Player attack: `ATTACK_TIME` 0.30 s, `ATTACK_COOLDOWN` 0.15 s, 1 damage. The
  hitbox is only monitoring while `attack_timer < ATTACK_TIME * 0.75`, and is
  offset 10 px in the facing direction.
* Damage to the player runs through `take_damage(amount, from_position)`:
  1.2 s of invincibility (sprite strobes at 12 Hz), knockback of
  (±80, -120), and a life lost at 0 HP.
* Enemies take damage when their `Hurtbox` sees an area in the
  `player_attack` group; the amount comes from `get_attack_damage()` on the
  area's parent. Death awards `score_value`, disables the boxes deferred, and
  frees the node.
* Contact damage is dealt by the enemy `Hitbox` via `body_entered`.
* The boss overrides `take_damage()` / `_die()` to emit `boss_hp_changed` and
  `boss_defeated` (9 HP, 1000 points); `hud.gd::attach_boss()` connects both.

## Level construction

`level_1.gd` builds everything procedurally instead of shipping a large scene
file: a `TileSet` with one 16 px atlas source (11 tiles, full-tile collision
polygons on brick/concrete/roof), a background layer, a foreground layer of six
platform rectangles across a 100x9 tile world, ladders and spikes assembled as
`Area2D` + `Sprite2D` + `CollisionShape2D` at runtime, then pickups, enemies,
the boss, the player (with a smoothed `Camera2D` limited to the world bounds)
and the HUD.

Ladder areas call `enter_ladder()` / `exit_ladder()` on the player, which keeps
an overlap counter rather than a boolean.

**Caveat:** `scenes/level_1.tscn` uses a `TileMap` node, which has been
deprecated since Godot 4.3 in favor of `TileMapLayer`. `projectile.gd` also
type-checks `body is TileMap`. Both still work in 4.7-beta but will need
migrating when `TileMap` is removed.

## Assets

Every PNG and WAV is generated by `tools/gen_assets.py` (requires Pillow):

```bash
python3 tools/gen_assets.py
```

Sprites are horizontal strips of equal-sized frames; `anim_util.gd::build()`
slices them into `AtlasTexture` frames of a `SpriteFrames` resource using the
per-entity `FRAMES_SPEC` constant declared in each script.
