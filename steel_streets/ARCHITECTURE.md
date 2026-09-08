# Steel Streets — Architecture

Technical reference for the `steel_streets/` Godot project bundled in this
repository. It is the authoritative description of how the game is wired
together; the gameplay overview and controls live in [README.md](README.md).

Everything described here is implemented in plain GDScript under
`scripts/` plus a handful of small `.tscn` files under `scenes/`. The project
does **not** use `AnimationPlayer`, `AnimationTree`, `ShaderMaterial`,
particles, or hand-authored tilemaps: animation is driven by
`AnimatedSprite2D` + `SpriteFrames` built at runtime, hit feedback is done by
toggling `modulate`/`visible`, and the level is generated procedurally in code.

## Runtime configuration (`project.godot`)

| Setting | Value | Why |
| :--- | :--- | :--- |
| `run/main_scene` | `res://scenes/title_screen.tscn` | Title is the entry point. |
| `config/features` | `4.3`, `GL Compatibility` | Authored for Godot 4.3+, `gl_compatibility` renderer. |
| `[autoload] GameManager` | `res://scripts/game_manager.gd` | Single global singleton. |
| `window/size/viewport_*` | 160 × 144 | Game Boy internal resolution. |
| `window/size/window_*_override` | 640 × 576 | 4× integer upscale by default. |
| `window/stretch/mode` / `aspect` | `viewport` / `keep` | Pixel-perfect scaling, letterboxed. |
| `default_texture_filter` | `0` (nearest) | No bilinear smoothing on sprites. |
| `2d/snap/snap_2d_*_to_pixel` | `true` | Avoid sub-pixel jitter. |

### Input actions

| Action | Keyboard | Gamepad |
| :--- | :--- | :--- |
| `move_left` / `move_right` | ← / → , A / D | Left stick X, D-pad left/right (buttons 13/14) |
| `jump` | Space, W | Button 0 (A) |
| `attack` | Z, J | Button 2 (X) |
| `climb_up` / `climb_down` | ↑ / ↓ , W / S | D-pad up/down (buttons 11/12) |
| `start` | Enter, Space | Button 7 (Start) |

Menu screens (`title_screen.gd`, `splash_screen.gd`, `game_over.gd`,
`victory.gd`) do not use actions; they advance on *any* pressed
`InputEventKey`, `InputEventJoypadButton`, or `InputEventMouseButton`.

### 2D physics layers

Eight named layers are defined in `[layer_names]`. Bit values are what appear
in the `.tscn` files and in `level_1.gd`.

| # | Bit | Name | Used by |
| :-: | :-: | :--- | :--- |
| 1 | 1 | `world` | TileSet physics layer 0 (solid tiles). |
| 2 | 2 | `player` | `Player` body. |
| 3 | 4 | `player_hitbox` | `Player/AttackHitbox` (Area2D). |
| 4 | 8 | `enemy` | Enemy bodies and their `Hurtbox`. |
| 5 | 16 | `enemy_hitbox` | Enemy `Hitbox` areas and `Projectile`. |
| 6 | 32 | `ladder` | Ladder `Area2D`s created by `level_1.gd`. |
| 7 | 64 | `pickup` | `QuestionBlock`. |
| 8 | 128 | `hazard` | Spike `Area2D`s created by `level_1.gd`. |

Collision matrix (layer → mask):

| Node | layer | mask | Meaning |
| :--- | :-: | :-: | :--- |
| `Player` (CharacterBody2D) | 2 | 1 | Walks on world only. |
| `Player/AttackHitbox` | 4 | 72 (8+64) | Hits enemy hurtboxes and pickups. |
| Enemy body | 8 | 1 | Walks on world only. |
| Enemy `Hurtbox` | 8 | 4 | Receives player attacks. |
| Enemy `Hitbox` | 16 | 2 | Deals contact damage to player. |
| `Projectile` | 16 | 3 (1+2) | Hurts player; despawns on world. |
| `QuestionBlock` | 64 | 6 (2+4) | Consumed by player body or attack. |
| Ladder area | 32 | 2 | Detects player overlap. |
| Spike area | 128 | 2 | Damages player. |

## `GameManager` autoload (`scripts/game_manager.gd`)

The only global state. Persists across `change_scene_to_file()` calls.

```gdscript
const STARTING_LIVES := 3
const STARTING_HP := 5

var lives: int
var score: int
var max_hp: int
var current_hp: int

signal score_changed(new_score: int)
signal lives_changed(new_lives: int)
signal hp_changed(current: int, maximum: int)

func reset_run() -> void           # called by title_screen._ready()
func add_score(amount: int) -> void
func set_hp(value: int) -> void    # clamps to [0, max_hp]
func heal(amount: int) -> void
func take_life() -> bool           # false when no lives remain; refills HP otherwise

func goto_title() / goto_splash() / goto_level() / goto_game_over() / goto_victory()
```

All scene transitions go through the `goto_*` helpers, which wrap
`get_tree().change_scene_to_file(<SCENE const>)`. Nothing else in the project
calls `change_scene_*` directly.

## Scene flow

```
title_screen ──any input──▶ splash_screen ──2.5 s or any input──▶ level_1
     ▲                                                              │
     │                       lives left ──▶ goto_level() (restart level, HP refilled)
     │                                                              │
     ├──any input── game_over ◀── player dies with 0 lives ─────────┤
     └──any input── victory   ◀── boss_defeated (+1.5 s) ───────────┘
```

* `title_screen.gd` calls `GameManager.reset_run()` in `_ready()`, so every
  visit to the title resets lives/score/HP.
* `splash_screen.gd` auto-advances via `create_timer(HOLD_TIME)`; input skips.
* `player._die()` calls `GameManager.take_life()`; on `true` it reloads the
  level after 1.0 s, on `false` it goes to game over after 1.2 s.
* `level_1._on_boss_defeated()` waits 1.5 s then `goto_victory()`.
* `victory.gd` shows `GameManager.score`; both terminal screens return to the
  title on any input.

## Level generation (`scripts/level_1.gd`, `scenes/level_1.tscn`)

`level_1.tscn` contains only three nodes: `TileMap`, `Entities` (Node2D) and
`Bgm` (AudioStreamPlayer). Everything else is created in `_ready()`:

1. `_build_tileset()` — builds a `TileSet` from `assets/sprites/tiles/tileset.png`
   (16 px tiles, one `TileSetAtlasSource`, physics layer 0 on collision layer
   `world`). Tiles listed in `SOLID_TILES` (`A_BRICK`, `A_CONCRETE`, `A_ROOF`)
   get a collision polygon.
2. `_paint_background()` / `_paint_foreground()` — paint TileMap layer 0
   (decor: sky, brick backdrop, windows, doors) and layer 1 (solid ground and
   platforms from the `PLATFORMS` rectangle list).
3. `_spawn_decor()` — `_make_ladder()` and `_make_spike()` create `Area2D`s
   with child `Sprite2D`s and a `CollisionShape2D`. Ladders call
   `enter_ladder()`/`exit_ladder()` on the overlapping body; spikes call
   `take_damage(1, pos)`.
4. `_spawn_pickups()`, `_spawn_enemies()`, `_spawn_boss()`, `_spawn_player()` —
   instantiate the `.tscn` scenes at positions from the `PICKUPS`,
   `FOOT_SOLDIERS`, `SHURIKEN_THROWERS`, `BOSS_POS`, `PLAYER_SPAWN` tables.
   A `Camera2D` with position smoothing and world-bound limits is added as a
   child of the player.
5. `_spawn_hud()` — instantiates `hud.tscn`; the boss is handed to
   `HUD.attach_boss()` via `call_deferred`.

World size is `WORLD_W × WORLD_H` = 100 × 9 tiles (1600 × 144 px), ground on
row 8. Level layout is edited by changing the constant tables at the top of
the script, not in the editor.

## Entities

All entity sprites use `AnimatedSprite2D`. Frames are built at runtime by
`AnimUtil.build(FRAMES_SPEC)` (`scripts/anim_util.gd`), which slices a
horizontal strip PNG into `AtlasTexture` frames and returns a `SpriteFrames`.
Each `FRAMES_SPEC` entry is `{name, path, count, w, h, fps, loop}`.

### Player (`scripts/player.gd`, `scenes/player.tscn`)

`CharacterBody2D` in group `player`. Children: `Sprite`, `CollisionShape2D`,
`AttackHitbox` (Area2D, group `player_attack`) with `Shape`, and
`SfxJump`/`SfxAttack`/`SfxHurt`.

* Movement: `Input.get_axis("move_left","move_right") * SPEED (60)`,
  gravity 700, `JUMP_VELOCITY -190`. Split into `_handle_grounded()` and
  `_handle_climbing()` depending on `climbing`.
* Climbing: `ladders_overlapping` counter maintained by
  `enter_ladder()`/`exit_ladder()`. Pressing `climb_up` (or `climb_down` while
  airborne) on a ladder enters climb mode; horizontal input or jump exits it.
* Attack: timer based, not animation-event based. `_start_attack()` sets
  `attack_timer = ATTACK_TIME (0.30 s)` and `cooldown_timer`;
  `_update_attack_state()` enables `AttackHitbox` only while
  `0 < attack_timer < ATTACK_TIME * 0.75` and moves it to `10 px * facing`.
  Enemies query damage via `get_attack_damage()` (returns `ATTACK_DAMAGE = 1`).
* Damage: `take_damage(amount, from_position)` is ignored while
  `invincible_timer > 0` or `dead`. Otherwise it writes HP into
  `GameManager.set_hp()`, sets `INVINCIBLE_TIME (1.2 s)`, applies knockback
  (`KNOCKBACK_X 80`, `KNOCKBACK_Y -120`) away from the source and cancels
  climbing. Invincibility is shown by strobing `sprite.visible` at 12 Hz.
* Death: `_die()` plays `hurt`, disables the hitbox, then defers to
  `GameManager.take_life()` (see Scene flow).
* `_ready()` calls `GameManager.set_hp(GameManager.max_hp)`, so HP refills on
  every level (re)load.

### `EnemyBase` (`scripts/enemy_base.gd`)

`CharacterBody2D` base for all enemies, group `enemy`. Exports `max_hp`,
`contact_damage`, `score_value`. Looks up optional children `Sprite`,
`Hurtbox`, `Hitbox`, `SfxHit` with `has_node()` guards.

* `Hurtbox.area_entered` → if the area is in group `player_attack`, call
  `take_damage(owner.get_attack_damage())`.
* `Hitbox.body_entered` → if the body is in group `player`, call
  `body.take_damage(contact_damage, global_position)`.
* `take_damage()` decrements `hp`, sets `hit_flash_timer = 0.18`; `_process()`
  flickers `sprite.modulate` between white-boost and normal.
* `_die()` sets `dead`, awards `score_value` via `GameManager.add_score()`,
  disables both areas with `set_deferred`, and `queue_free()`s. There is no
  death animation.

Subclasses call `super()` from `_ready()` **after** overriding the exported
stats, then assign their `SpriteFrames`.

| Enemy | Script | HP | Score | Behaviour |
| :--- | :--- | :-: | :-: | :--- |
| Foot soldier | `foot_soldier.gd` | 2 | 100 | Patrols at 22 px/s, turns at walls or ledges (`_has_floor_ahead()` raycast on layer `world`), chases at 35 px/s when player within 70 px horizontally and 24 px vertically. |
| Shuriken thrower | `shuriken_thrower.gd` | 3 | 150 | Paces ±14 px around spawn at 12 px/s; when player within 110 px × 32 px, faces them and fires a `Projectile` every 1.6 s via `_fire()` (adds to its parent). |
| Boss | `boss.gd` | 9 | 1000 | Hand-written `enum State { PAUSE, WINDUP, CHARGE, RECOVER }` cycle (1.0 / 0.5 / 1.0 / 0.7 s). Charges at 70 px/s toward the player, stops early on `is_on_wall()`. Emits `boss_hp_changed(current, max)` and `boss_defeated`; joins group `boss`. |

### Projectile (`scripts/projectile.gd`)

`Area2D` on layer `enemy_hitbox`. Moves `direction * 110 px/s` in
`_physics_process`, lives 3 s. On `body_entered`: damages a `player` body
(`DAMAGE = 1`) or despawns on `TileMap`/`StaticBody2D`.

### QuestionBlock (`scripts/question_block.gd`)

`Area2D` on layer `pickup`. Consumed either by the player body entering or a
`player_attack` area entering. Awards `score_reward` (50), heals
`heal_amount` (1) through `GameManager.heal()`, hides the sprite, disables
monitoring, awaits `SfxPickup.finished`, then `queue_free()`s. There is no
spawned pickup item and no "hit from below" check.

### HUD (`scripts/hud.gd`, `scenes/hud.tscn`)

`CanvasLayer` instantiated by the level. Subscribes to the three
`GameManager` signals in `_ready()` and immediately renders current values:

* Hearts: rebuilds `Root/Top/Hearts` with one `TextureRect` per `max_hp`
  (`heart.png` / `heart_empty.png`).
* `Root/Top/Stats/Lives` → `"x%d"`, `Root/Top/Stats/Score` → `"%05d"`.
* `attach_boss(boss)` shows `Root/BossBar` and connects `boss_hp_changed`
  (scales `Fill.size.x` over 64 px) and `boss_defeated` (hides the bar).

## Assets and tooling

* `tools/gen_assets.py` regenerates every PNG under `assets/sprites/` and every
  WAV under `assets/audio/` from code using the 4-colour Game Boy palette
  (`#e0f8d0`, `#88c070`, `#346856`, `#081820`). Requires Pillow. Run from the
  repo root: `python3 steel_streets/tools/gen_assets.py`.
* Sprite sheets are horizontal strips; frame counts and sizes must match the
  `FRAMES_SPEC` tables in the scripts that load them.
* `.import` sidecar files are committed; the `.godot/` cache is ignored.

## Running

```bash
# From the repo root, with any Godot 4.3+ binary (or one built from this tree):
godot --path steel_streets
```

There is no test suite for the game itself; the engine's C++ tests under
`tests/` do not cover `steel_streets/`.
