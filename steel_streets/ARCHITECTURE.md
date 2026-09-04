# Steel Streets — Architecture

Reference for how the bundled game project in `steel_streets/` is wired
together. It complements `README.md` (how to run/play) and describes the
actual code so documentation tools and new contributors don't have to infer it.

Everything gameplay-related is plain GDScript under `scripts/`; there are no
`AnimationPlayer`/`AnimationTree` resources, shaders, or particle nodes. All
animation is `AnimatedSprite2D` driven by `SpriteFrames` built at runtime, and
all state machines are hand-written timers in `_physics_process`.

## Scene flow

`GameManager` (`scripts/game_manager.gd`, autoload registered in
`project.godot`) is the only place that calls
`get_tree().change_scene_to_file()`. Every other scene calls one of its
`goto_*` helpers:

```
title_screen.tscn ──(any key/button/click)──► splash_screen.tscn
      ▲                                              │ 2.5 s timer or any input
      │                                              ▼
      │                                        level_1.tscn
      │                                         │          │
      │            player HP 0 & lives left ────┘          │ boss_defeated
      │            (goto_level = restart level)            ▼
      ├──(any input)── game_over.tscn ◄── HP 0 & no lives  victory.tscn ──(any input)──┘
```

| Scene | Root type | Script | Notes |
| --- | --- | --- | --- |
| `title_screen.tscn` | `Control` | `title_screen.gd` | `_ready()` calls `GameManager.reset_run()`; blinks "PRESS START". |
| `splash_screen.tscn` | `Control` | `splash_screen.gd` | Auto-advances after `HOLD_TIME = 2.5` s or on input. |
| `level_1.tscn` | `Node2D` | `level_1.gd` | Only `TileMap`, `Entities`, `Bgm` nodes in the `.tscn`; everything else is built in code. |
| `hud.tscn` | `CanvasLayer` (layer 10) | `hud.gd` | Instantiated by `level_1.gd`, not part of the level scene file. |
| `game_over.tscn` / `victory.tscn` | `Control` | `game_over.gd` / `victory.gd` | Any input → `goto_title()`. Victory shows `FINAL SCORE %05d`. |

## GameManager API

```gdscript
const STARTING_LIVES := 3
const STARTING_HP := 5
var lives, score, max_hp, current_hp

signal score_changed(new_score: int)
signal lives_changed(new_lives: int)
signal hp_changed(current: int, maximum: int)

func reset_run()                 # title screen
func add_score(amount: int)      # enemies, question blocks
func set_hp(value: int)          # player.take_damage (clamped 0..max_hp)
func heal(amount: int)           # question blocks
func take_life() -> bool         # player._die; true = respawn, false = game over
func goto_title() / goto_splash() / goto_level() / goto_game_over() / goto_victory()
```

Health lives in `GameManager`, not on the player: `player.gd` reads
`GameManager.current_hp` and writes back via `set_hp()`, so HP survives the
level reload that `take_life()` triggers.

## Physics layers (from `project.godot` `[layer_names]`)

| Bit | Layer | Name | Who sets `collision_layer` here |
| --- | --- | --- | --- |
| 1 | 1 | `world` | `TileSet` physics layer 0 (solid tiles) |
| 2 | 2 | `player` | `Player` body |
| 4 | 3 | `player_hitbox` | `Player/AttackHitbox` (Area2D, group `player_attack`) |
| 8 | 4 | `enemy` | Enemy bodies and their `Hurtbox` |
| 16 | 5 | `enemy_hitbox` | Enemy `Hitbox` and `Projectile` |
| 32 | 6 | `ladder` | Ladder Area2Ds created by `level_1.gd` |
| 64 | 7 | `pickup` | `QuestionBlock` |
| 128 | 8 | `hazard` | Spike Area2Ds created by `level_1.gd` |

Masks that matter:

- `Player/AttackHitbox.collision_mask = 72` (enemy `8` + pickup `64`).
- Enemy `Hurtbox.collision_mask = 4` (player_hitbox); enemy
  `Hitbox.collision_mask = 2` (player body).
- `Projectile.collision_mask = 3` (world + player).
- `QuestionBlock.collision_mask = 6` (player body + player_hitbox).
- Ladders/spikes mask `2` (player body only).

## Damage contracts (duck-typed, no shared interface)

| Call | Implemented by | Called from |
| --- | --- | --- |
| `take_damage(amount: int, from_position: Vector2)` | `player.gd` | `enemy_base._on_hitbox_body_entered`, `projectile.gd`, spike lambda in `level_1.gd` |
| `take_damage(amount: int)` | `enemy_base.gd` (overridden in `boss.gd` to emit `boss_hp_changed`) | `enemy_base._on_hurtbox_area_entered` |
| `get_attack_damage() -> int` | `player.gd` (returns `ATTACK_DAMAGE = 1`) | `enemy_base.gd` (looks up the hitbox's parent), `question_block.gd` (used as an "is this the player" check) |
| `enter_ladder()` / `exit_ladder()` | `player.gd` (counts overlapping ladders) | ladder `body_entered/exited` in `level_1.gd` |

Groups used for lookups: `player`, `player_attack`, `enemy`, `boss`, `ladder`.
`get_tree().get_first_node_in_group("player")` is how enemies find the player.

## Entities

### Player (`player.gd`, `CharacterBody2D`, layer 2 / mask 1)

- Movement: `Input.get_axis("move_left", "move_right")`, `SPEED = 60`,
  `JUMP_VELOCITY = -190`, `GRAVITY = 700`.
- Attack: `_start_attack()` sets `attack_timer = 0.30` and
  `cooldown_timer = 0.45`. `_update_attack_state()` enables the
  `AttackHitbox` only while `attack_timer < 0.75 * ATTACK_TIME` (the swing
  window) and flips it to `10 * facing` px. No animation tracks involved.
- Climbing: `climbing` becomes true when `ladders_overlapping > 0` and
  `climb_up`/`climb_down` is held; horizontal input, jump, or leaving the
  ladder area exits it.
- Damage: `INVINCIBLE_TIME = 1.2` s during which `take_damage` is ignored and
  the sprite strobes at 12 Hz via `sprite.visible`. Knockback is
  `(±80, -120)`. Death calls `GameManager.take_life()` then `goto_level()`
  (1.0 s) or `goto_game_over()` (1.2 s).

### EnemyBase (`enemy_base.gd`, `CharacterBody2D`)

Exports `max_hp`, `contact_damage`, `score_value`; subclasses set them in
`_ready()` *before* calling `super()`. Provides the optional child lookups
(`Sprite`, `Hurtbox`, `Hitbox`, `SfxHit`), hit flash (`sprite.modulate`
toggled for 0.18 s — no shader), and `_die()` (add score, disable areas,
`queue_free()`).

| Subclass | HP | Score | Behaviour |
| --- | --- | --- | --- |
| `foot_soldier.gd` | 2 | 100 | Patrols at 22 px/s, turns at walls or when `_has_floor_ahead()` raycast (mask 1) finds no floor; chases at 35 px/s within 70 px horizontally / 24 px vertically. |
| `shuriken_thrower.gd` | 3 | 150 | Paces ±14 px around spawn at 12 px/s; every 1.6 s fires `projectile.tscn` toward the player if within 110 px / 32 px. |
| `boss.gd` | 9 | 1000 | `enum State { PAUSE, WINDUP, CHARGE, RECOVER }` timer FSM (1.0 / 0.5 / 1.0 / 0.7 s). Charges at 70 px/s, stops on `is_on_wall()`. Emits `boss_hp_changed(current, maximum)` and `boss_defeated`. |

### Projectile (`projectile.gd`, `Area2D`, layer 16 / mask 3)

Moves `direction * 110` px/s in `_physics_process`, lives 3 s, frees on
player contact (after `take_damage`) or on `TileMap`/`StaticBody2D` contact.
`direction` is set by the thrower via `p.set("direction", face_dir)` before
`add_child`.

### QuestionBlock (`question_block.gd`, `Area2D`, layer 64 / mask 6)

Consumed either by the player body entering or a `player_attack` area
entering (no "hit from below" check). Awards `score_reward = 50`, heals
`heal_amount = 1`, hides the sprite, waits for `SfxPickup` to finish, then
`queue_free()`. It does not spawn a separate pickup item.

### AnimUtil (`anim_util.gd`, static)

`AnimUtil.build(specs) -> SpriteFrames`: each spec is
`{name, path, count, w, h, fps, loop}` describing a horizontal strip PNG;
frames are `AtlasTexture` regions `Rect2(i * w, 0, w, h)`. Every entity
declares a `FRAMES_SPEC` constant and assigns
`sprite.sprite_frames = AnimUtil.build(FRAMES_SPEC)` in `_ready()`.

## Level generation (`level_1.gd`)

The level is 100×9 tiles of 16 px (`WORLD_W`, `WORLD_H`, `TILE`), row 8 is
ground. `_ready()` runs, in order:

1. `_build_tileset()` — `TileSet` with one `TileSetAtlasSource` over
   `assets/sprites/tiles/tileset.png` (11 tiles in one row); `A_BRICK`,
   `A_CONCRETE`, `A_ROOF` get a full-tile collision polygon on physics layer 0
   (collision layer `1` = world).
2. `_paint_background()` (TileMap layer 0: sky rows 0–3, brick wall rows 4–7,
   windows/doors/clouds) and `_paint_foreground()` (layer 1, rectangles from
   `PLATFORMS`).
3. `_spawn_decor()` — ladders (`LADDERS`, Area2D layer 32 with stacked
   `Sprite2D`s) and spikes (`SPIKES`, Area2D layer 128 that calls
   `take_damage(1, ...)` on the player).
4. `_spawn_pickups()`, `_spawn_enemies()`, `_spawn_player()` (also creates a
   `Camera2D` with smoothing, clamped to world bounds, as a child of the
   player), `_spawn_boss()` (connects `boss_defeated` → 1.5 s → `goto_victory()`
   and defers `hud.attach_boss(boss)`), `_spawn_hud()`.

Level layout data are the `PLATFORMS`, `LADDERS`, `SPIKES`, `PICKUPS`,
`FOOT_SOLDIERS`, `SHURIKEN_THROWERS`, `BOSS_POS`, `PLAYER_SPAWN` constants at
the top of the file — edit those to change the level.

## HUD (`hud.gd`)

Connects to the three `GameManager` signals in `_ready()` and re-renders:
hearts as `TextureRect`s (`heart.png` / `heart_empty.png`), lives as `x%d`,
score as `%05d`. `attach_boss(boss)` shows `Root/BossBar` and connects
`boss_hp_changed` (fill width `64 * ratio`) and `boss_defeated` (hide).

## Rendering / project settings

- 160×144 viewport, 640×576 window, `stretch/mode = viewport`,
  `aspect = keep`, nearest texture filter, 2D pixel snapping.
- `renderer/rendering_method = gl_compatibility` (desktop and mobile).
- `config/features = ["4.3", "GL Compatibility"]` — the project targets the
  Godot 4.3 project format, independent of the engine version in the root
  `version.py`.

## Assets

All PNGs and WAVs are generated by `tools/gen_assets.py` (Pillow + stdlib
`wave`) using the 4-colour Game Boy palette `#e0f8d0 #88c070 #346856 #081820`.
Regenerate with `python3 steel_streets/tools/gen_assets.py` from the repo
root; `.import` sidecar files are committed, `.godot/` is ignored.
