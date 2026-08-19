# Steel Streets — Architecture Reference

Ground-truth reference for the bundled game project in this fork. Everything
below is taken from the files under `steel_streets/`; engine behaviour is only
described where the game relies on it.

## Where the code lives

| Path | Contents |
| --- | --- |
| `project.godot` | Autoload, 160x144 viewport, input map, physics layer names, GL Compatibility renderer |
| `scenes/*.tscn` | 12 scenes: 5 flow screens (title, splash, level_1, game_over, victory), HUD, and 6 entity scenes |
| `scripts/*.gd` | 15 GDScript files, ~1.2k lines total |
| `assets/sprites`, `assets/audio` | 4-colour PNG strips and WAV SFX/BGM, all generated |
| `tools/gen_assets.py` | Regenerates every PNG and WAV from code (Pillow + `wave`) |

## Engine version

`project.godot` declares `config/features=PackedStringArray("4.3", "GL Compatibility")`,
so the project loads in Godot 4.3 and newer, including the engine built from this
repository (`version.py`: **4.7-beta**). `scenes/level_1.tscn` still uses the
`TileMap` node, which is deprecated in favour of `TileMapLayer` — it works today
but is the one forward-compatibility risk in the project.

Run it against a local build:

```bash
./bin/godot.linuxbsd.editor.dev.x86_64 --path steel_streets
```

## Global state: `GameManager` (`scripts/game_manager.gd`)

Autoloaded `Node` (registered as `GameManager` in `project.godot`). It owns run
state only — it does not read input, drive frames, or touch the HUD directly.

State: `lives` (starts 3), `score`, `max_hp`/`current_hp` (start 5).

Signals: `score_changed(new_score)`, `lives_changed(new_lives)`,
`hp_changed(current, maximum)`.

API: `reset_run()`, `add_score(amount)`, `set_hp(value)` (clamped), `heal(amount)`,
`take_life()` → `false` when lives run out, and the navigation helpers
`goto_title()` / `goto_splash()` / `goto_level()` / `goto_game_over()` /
`goto_victory()`, each a `change_scene_to_file()` call.

## Scene flow

```
title_screen (reset_run, blinking PRESS START, any input)
  -> splash_screen (2.5 s hold, skippable)
    -> level_1
        player death & lives left  -> reload level_1 after 1.0 s
        player death & no lives    -> game_over -> title
        boss_defeated              -> victory (after 1.5 s) -> title
```

`title_screen`, `splash_screen`, `game_over` and `victory` all advance on any
key, joypad button, or mouse button via `_input()`.

## Level 1 is built in code (`scripts/level_1.gd`, 307 lines)

`level_1.tscn` contains only `TileMap`, `Entities`, and `Bgm`. `_ready()`
constructs everything else:

- `_build_tileset()` creates a `TileSet` (16 px tiles) from the 11-tile atlas
  strip and adds a full-tile collision polygon to the three solid tiles
  (brick, concrete, roof) on physics layer 1 (`world`).
- Two tilemap layers: layer 0 background (sky, brick wall, windows, doors,
  clouds), layer 1 the solid foreground painted from the `PLATFORMS` rectangles.
- World is `WORLD_W = 100` x `WORLD_H = 9` tiles (1600x144 px); ground row 8.
- Content tables at the top of the file are the level design: `PLATFORMS`,
  `LADDERS`, `SPIKES`, `PICKUPS`, `FOOT_SOLDIERS`, `SHURIKEN_THROWERS`,
  `BOSS_POS`, `PLAYER_SPAWN`. Editing those constants edits the level.
- Ladders and spikes are assembled at runtime as `Area2D` + `Sprite2D`
  (no scene files). Ladders call `enter_ladder()` / `exit_ladder()` on the
  player; spikes deal 1 damage via a lambda connected to `body_entered`.
- The `Camera2D` is created in `_spawn_player()` as a child of the player, with
  smoothing (speed 8) and limits clamped to the world bounds.
- The boss is handed to the HUD with `call_deferred("_attach_boss_to_hud", boss)`
  so the HUD is ready first; `boss_defeated` triggers the victory transition.

## Player (`scripts/player.gd`)

`CharacterBody2D`, group `player`. Everything is hand-rolled in
`_physics_process()`; there is no `AnimationPlayer`, `AnimationTree`, or state
machine resource in the project.

- Tunables: `SPEED 60`, `JUMP_VELOCITY -190`, `GRAVITY 700`, `CLIMB_SPEED 50`,
  `KNOCKBACK 80/-120`, `INVINCIBLE_TIME 1.2`, `ATTACK_TIME 0.30`,
  `ATTACK_COOLDOWN 0.15`, `ATTACK_DAMAGE 1`.
- Movement reads `Input.get_axis("move_left", "move_right")` and
  `Input.is_action_just_pressed(...)` directly.
- Attack: `_update_attack_state()` enables `AttackHitbox` (an `Area2D` in group
  `player_attack`) only while `attack_timer < ATTACK_TIME * 0.75`, and offsets it
  `±10 px` by facing. Damage is pulled by the target via `get_attack_damage()`.
- Ladders: `ladders_overlapping` counts overlapping ladder areas; climbing ends
  on horizontal input, on jump (0.7x jump velocity), or when the count hits 0.
- Damage: `take_damage(amount, from_position)` ignores hits while invincible,
  applies knockback away from the source, and strobes `sprite.visible` at 12 Hz
  for the i-frame window. On 0 HP it calls `GameManager.take_life()` and routes
  to a level reload or game over.
- Animation is 6 `AnimatedSprite2D` animations (idle, walk, jump, attack, climb,
  hurt) built by `AnimUtil` from 16x24 strips.

## Enemies

`scripts/enemy_base.gd` (`CharacterBody2D`, group `enemy`) owns the shared parts:
`max_hp` / `contact_damage` / `score_value` exports, `Hurtbox` (takes player
attacks), `Hitbox` (deals contact damage to the player), `take_damage()`, the
hit flash, and `_die()` (award score, disable both areas deferred, `queue_free()`).
The hit flash is a plain `modulate` toggle between `Color(2,2,2)` and white for
0.18 s — no shader material is involved.

| Enemy | Script | HP | Score | Behaviour |
| --- | --- | --- | --- | --- |
| Foot soldier | `foot_soldier.gd` | 2 | 100 | Patrols at 22 px/s, chases at 35 px/s within 70 px; turns at walls and at ledges detected by a downward `intersect_ray` probe |
| Shuriken thrower | `shuriken_thrower.gd` | 3 | 150 | Paces ±14 px around its spawn `x`, fires `projectile.tscn` every 1.6 s while the player is within 110 px |
| Boss | `boss.gd` | 9 | 1000 | `enum State { PAUSE, WINDUP, CHARGE, RECOVER }` timer-driven cycle (1.0 / 0.5 / 1.0 / 0.7 s), charges at 70 px/s and recovers early on wall contact |

The boss additionally emits `boss_hp_changed(current, maximum)` and
`boss_defeated`, and overrides `take_damage()` / `_die()` to fire them.

Other combat entities:

- `projectile.gd` — `Area2D` shuriken, 110 px/s, 3 s lifetime, 1 damage,
  despawns on the player, `TileMap`, or `StaticBody2D`.
- `question_block.gd` — `Area2D` pickup, +50 score and +1 HP, consumed by walking
  into it or hitting it with the attack hitbox; waits for its SFX before freeing.

## HUD (`scripts/hud.gd`)

`CanvasLayer` that connects to the three `GameManager` signals in `_ready()`,
rebuilds the heart row from `heart.png` / `heart_empty.png` on every HP change,
formats score as `%05d` and lives as `x%d`. `attach_boss(boss)` reveals the boss
bar and resizes `Fill` (64 px wide at full HP) from `boss_hp_changed`.

## Physics layers (from `project.godot`)

| Bit | Name | Used by |
| --- | --- | --- |
| 1 | `world` | TileSet physics layer; every body masks it |
| 2 | `player` | Player body; masked by enemy hitboxes, ladders, spikes, pickups, projectiles |
| 3 | `player_hitbox` | `AttackHitbox` (mask 72 = enemy + pickup) |
| 4 | `enemy` | Enemy bodies and hurtboxes |
| 5 | `enemy_hitbox` | Enemy contact hitboxes and projectiles |
| 6 | `ladder` | Runtime ladder areas |
| 7 | `pickup` | Question blocks |
| 8 | `hazard` | Runtime spike areas |

Node groups used for lookups: `player`, `player_attack`, `enemy`, `boss`, `ladder`.

## Asset pipeline

`AnimUtil.build(specs)` (`scripts/anim_util.gd`) turns a horizontal PNG strip
into a `SpriteFrames` resource by slicing `AtlasTexture` regions — each entity
declares its own `FRAMES_SPEC` constant (`name`, `path`, `count`, `w`, `h`,
`fps`, `loop`) and assigns the result to its `AnimatedSprite2D` in `_ready()`.

All art and audio are reproducible:

```bash
python3 steel_streets/tools/gen_assets.py   # needs Pillow
```

Rendering settings that keep the look: `default_texture_filter=0` (nearest),
`stretch/mode=viewport` with `aspect=keep`, 2D transform and vertex pixel
snapping, and the 4-colour palette `#e0f8d0 / #88c070 / #346856 / #081820`.
