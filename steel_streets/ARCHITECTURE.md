# Steel Streets — Architecture Reference

This is the **authoritative engineering reference** for the `steel_streets/`
sample project. It is intended as the source of truth for the team and for any
generated documentation (e.g. DeepWiki) so everyone is working from the same
context.

If something in another doc (a wiki page, a PR description, a knowledge note)
disagrees with this file, this file is correct — please update the other place.

---

## 1. What Steel Streets is

Steel Streets is a **standalone Godot 4 sample project** that lives inside the
`steel_streets/` folder of this Godot fork. It is *not* an engine smoke test
and is *not* used by upstream Godot CI. It is a self-contained beat-'em-up
platformer in the style of the original Game Boy *Teenage Mutant Ninja Turtles*
games.

Run it with:

```bash
godot --path steel_streets
```

Key visual constraints:

- **Internal viewport:** 160 × 144 pixels (true Game Boy resolution).
- **Window override:** 640 × 576 (a 4× integer scale of the viewport).
- **Palette:** the 4 classic Game Boy greens — `#e0f8d0`, `#88c070`, `#346856`,
  `#081820`. Nothing in the project ever draws colours outside this palette.
- **Renderer:** GL Compatibility (set in `project.godot` via
  `config/features=PackedStringArray("4.3", "GL Compatibility")` and
  `rendering/renderer/rendering_method="gl_compatibility"`).

---

## 2. Project configuration (`project.godot`)

The configuration is small and intentional. The parts that matter for
gameplay/look:

| Key | Value | Why |
| --- | --- | --- |
| `application/run/main_scene` | `res://scenes/title_screen.tscn` | Game starts on the title. |
| `application/config/features` | `("4.3", "GL Compatibility")` | Targets Godot 4.3 with the compatibility renderer. |
| `autoload/GameManager` | `*res://scripts/game_manager.gd` | Singleton autoload (the `*` prefix enables it). |
| `display/window/size/viewport_width` | `160` | Native horizontal resolution. |
| `display/window/size/viewport_height` | `144` | Native vertical resolution. |
| `display/window/size/window_width_override` | `640` | 4× window width on desktop. |
| `display/window/size/window_height_override` | `576` | 4× window height on desktop. |
| `display/window/stretch/mode` | `viewport` | Renders at 160×144 then scales the whole framebuffer up. |
| `display/window/stretch/aspect` | `keep` | Letterboxes to preserve the GB aspect ratio. |
| `rendering/textures/canvas_textures/default_texture_filter` | `0` | Nearest-neighbour filtering globally — no per-import overrides needed. |

**Input map** (`[input]` section). Six actions, each bound to keyboard,
arrow/WASD, and a gamepad equivalent:

| Action | Keys | Gamepad |
| --- | --- | --- |
| `move_left` | `A`, `←` | left stick / d-pad left |
| `move_right` | `D`, `→` | left stick / d-pad right |
| `jump` | `Space`, `W` | A button |
| `attack` | `Z`, `J` | X button |
| `climb_up` | `W`, `↑` | d-pad up |
| `climb_down` | `S`, `↓` | d-pad down |

Note: `move_up` / `move_down` do **not** exist as actions. Vertical movement
on ladders uses the dedicated `climb_up` / `climb_down` actions, which
deliberately overlap with `jump` (`W`) so the player only needs three buttons
in the worst case.

---

## 3. Physics layers (the real ones)

These layer values are derived from the actual `collision_layer` /
`collision_mask` bitmasks set on each scene and on the helper bodies created
in `level_1.gd`. **Do not invent new layer numbers — use this table.**

| # | Bit | Value | Used by | Source |
| --- | --- | --- | --- | --- |
| 1 | 0 | 1 | World / static tile geometry | `level_1.gd` (`set_physics_layer_collision_layer(0, 1)`) |
| 2 | 1 | 2 | Player body | `scenes/player.tscn` (`collision_layer=2`) |
| 3 | 2 | 4 | Player attack hitbox | `scenes/player.tscn` (`AttackHitbox.collision_layer=4`) |
| 4 | 3 | 8 | Enemy body / enemy hurtbox | `scenes/foot_soldier.tscn`, `boss.tscn`, `shuriken_thrower.tscn` |
| 5 | 4 | 16 | Enemy contact hitbox / projectile body | enemy `Hitbox` Area2Ds, `scenes/projectile.tscn` |
| 6 | 5 | 32 | Ladder Area2D | `level_1.gd` `_make_ladder()` |
| 7 | 6 | 64 | Question-block pickup | `scenes/question_block.tscn` (`collision_layer=64`) |
| 8 | 7 | 128 | Hazard (spikes) | `level_1.gd` `_make_spike()` |

Cross-checks worth keeping in mind when editing:

- The **player attack hitbox** masks `4 + 64 = 72` (enemy hurtboxes + question
  blocks) — punching a question block is intentional.
- The **enemy hitbox** masks `2` (player body only).
- The **projectile** masks `1 + 2 = 3` (world + player) so it despawns on
  walls and damages the player.
- The **question block** masks `2 + 4 = 6` so it consumes on body-touch *or*
  on player-attack.

---

## 4. Scene flow

```
title_screen.tscn  ──any input──▶  splash_screen.tscn
                                          │
                                          │ HOLD_TIME (2.5 s) or input
                                          ▼
                                    level_1.tscn
                                  ┌─────┴─────┐
                       boss_defeated         out of lives
                                  │             │
                                  ▼             ▼
                            victory.tscn   game_over.tscn
                                  │             │
                                  └──any input──┴──▶ title_screen.tscn
```

Every transition goes through one of the `GameManager.goto_*` helpers — there
are no `change_scene_to_file` calls scattered through individual scripts. This
is the single chokepoint for adding loading screens, save points, etc. later.

---

## 5. `GameManager` (the autoload)

`scripts/game_manager.gd` extends `Node` and is registered via
`autoload/GameManager="*res://scripts/game_manager.gd"`. It is the only piece
of state that survives scene transitions.

### State

| Field | Default | Notes |
| --- | --- | --- |
| `lives` | `3` (`STARTING_LIVES`) | Decremented by `take_life()`. |
| `score` | `0` | Bumped by `add_score(amount)`. |
| `max_hp` | `5` (`STARTING_HP`) | Effectively constant for now. |
| `current_hp` | `5` | Clamped to `[0, max_hp]` in `set_hp`. |

### Signals (the **real** names — match these exactly when subscribing)

| Signal | Args | Emitted from |
| --- | --- | --- |
| `score_changed` | `(new_score: int)` | `add_score`, `reset_run` |
| `lives_changed` | `(new_lives: int)` | `take_life`, `reset_run` |
| `hp_changed` | `(current: int, maximum: int)` | `set_hp`, `heal`, `take_life`, `reset_run` |

There is **no** `player_hit`, `enemy_died`, or `health_changed` signal — past
docs that mention them are stale. The HUD (`hud.gd`) and the boss bar are the
only consumers.

### Methods

- `reset_run()` — called by `title_screen.gd._ready()` to start fresh.
- `add_score(amount)` — bumps score and emits `score_changed`.
- `set_hp(value)` / `heal(amount)` — both emit `hp_changed`.
- `take_life()` — returns `false` if the player is out of lives. The caller
  (`player.gd._die`) uses that return value to choose between
  `goto_level()` (respawn) and `goto_game_over()`.
- `goto_title / goto_splash / goto_level / goto_game_over / goto_victory` —
  thin wrappers over `get_tree().change_scene_to_file()`.

---

## 6. Player controller (`scripts/player.gd`)

Type: `CharacterBody2D`. The state machine lives entirely in
`_physics_process`, which dispatches to `_handle_grounded` or
`_handle_climbing` and then runs three update passes (attack window,
animation, invincibility flash).

### Tunables (all `const` at the top of the script)

| Constant | Value | Purpose |
| --- | --- | --- |
| `SPEED` | `60.0` | Horizontal walk speed. |
| `JUMP_VELOCITY` | `-190.0` | Initial Y velocity on a fresh jump. |
| `GRAVITY` | `700.0` | Same gravity used by enemies. |
| `CLIMB_SPEED` | `50.0` | Speed up/down on ladders. |
| `KNOCKBACK_X` / `KNOCKBACK_Y` | `80.0`, `-120.0` | Hit reaction. |
| `INVINCIBLE_TIME` | `1.2` s | Strobe duration after taking a hit. |
| `ATTACK_TIME` | `0.30` s | Total attack animation length. |
| `ATTACK_COOLDOWN` | `0.15` s | Extra cooldown on top of `ATTACK_TIME`. |
| `ATTACK_DAMAGE` | `1` | Returned by `get_attack_damage()`. |

### Attack window

`AttackHitbox.monitoring` is only enabled while
`attack_timer ∈ (0, ATTACK_TIME * 0.75)` — i.e. roughly the **middle 25 %** of
the swing. The hitbox is positioned at `±10 px` on the X axis based on
`facing`. This is the contract `enemy_base.gd` relies on when its Hurtbox
fires `area_entered` for the `player_attack` group.

### Ladders

Ladder support is independent of the climb animation and lives entirely on
the player. The level (`level_1.gd._make_ladder`) creates an Area2D and wires
it to `player.enter_ladder()` / `exit_ladder()`, which keep a
`ladders_overlapping` counter. The player only enters climb mode when the
counter is > 0 **and** the player presses `climb_up` (or `climb_down` while
not on the floor). Pressing left/right or `jump` while climbing exits climb
mode — the small jump impulse off a ladder is `JUMP_VELOCITY * 0.7`.

### Invincibility flash

Implemented as a 12 Hz strobe by toggling `sprite.visible` based on
`int(invincible_timer * 12.0) % 2`. There is no shader, no `AnimationPlayer`
track, and no `modulate` change.

### Death / respawn

`take_damage()` clamps via `GameManager.set_hp` and, if HP hits 0, calls
`_die()`, which calls `GameManager.take_life()` and then either reloads the
level (`goto_level`) or transitions to game-over (`goto_game_over`) after a
short timer. The player's own death animation is the existing `hurt` frame —
there is no separate death animation.

---

## 7. Enemies

All enemies extend `enemy_base.gd` (`extends CharacterBody2D`). The base owns:

- `max_hp`, `contact_damage`, `score_value` (all `@export`-able).
- `hurtbox` (`Area2D`, layer 4) and `hitbox` (`Area2D`, layer 5). Hurtbox
  reacts to anything in the `player_attack` group; hitbox damages anything in
  the `player` group.
- `take_damage(amount)` — flashes white via `modulate = Color(2, 2, 2, 1)` for
  `0.18 s` and emits `_die()` when HP hits 0.
- `_die()` — adds `score_value` to `GameManager`, deferred-disables both
  Areas, then `queue_free()`. There is no separate `enemy_died` signal — the
  score change is the observable event.

### Foot soldier (`foot_soldier.gd`)

- 2 HP, 1 contact damage, 100 score.
- Patrols using **wall + ledge detection** — turns at walls (`is_on_wall()`)
  or when a 16 px raycast 8 px ahead doesn't hit the ground. There are
  no fixed waypoints.
- Enters a "chase" state when the player is within `CHASE_RANGE = 70 px`
  horizontally and `24 px` vertically — speed bumps from `22 → 35 px/s`.

### Shuriken thrower (`shuriken_thrower.gd`)

- 3 HP, 1 contact damage, 150 score.
- **Paces** in a 14 px window around its spawn point (`PACE_SPEED = 12`).
  This is intentional — it does not chase.
- Fires a `projectile.tscn` instance every `FIRE_INTERVAL = 1.6 s` while the
  player is within `SIGHT_RANGE = 110 px` horizontally and `32 px` vertically.
- The first shot is offset by half an interval so two adjacent throwers don't
  fire in lock-step.

### Boss (`boss.gd`)

The boss has its own minimal state machine — **PAUSE → WINDUP → CHARGE →
RECOVER → PAUSE**. It does **not** jump, summon minions, or have multiple
phases.

| State | Duration | Behaviour |
| --- | --- | --- |
| `PAUSE` | `1.0` s | Stops, faces the player. |
| `WINDUP` | `0.5` s | Steps slightly *away* from the player and tints brighter (`modulate = (1.4, 1.4, 1.4, 1)`). |
| `CHARGE` | `1.0` s (or until `is_on_wall()`) | Runs toward the player at `CHARGE_SPEED = 70 px/s`. |
| `RECOVER` | `0.7` s | Stops, then loops back to `PAUSE`. |

Stats: 9 HP (`max_hp = 9`), 1 contact damage, 1000 score. The boss emits
**two extra signals** that are not on the base:

- `boss_hp_changed(current: int, maximum: int)` — drives the HUD's boss bar.
- `boss_defeated()` — the level listens for this and starts the
  `goto_victory()` transition after a 1.5 s delay.

---

## 8. Question blocks (`question_block.gd`)

Type: `Area2D` on layer 7 (value 64), masking the player body and the player
attack. Two `@export`s:

- `score_reward` (default `50`)
- `heal_amount` (default `1`)

On consume the block adds score, heals if `heal_amount > 0`, plays the pickup
SFX, and `queue_free()`s itself. **It does not spawn a coin, power-up, or any
other entity** — the heal *is* the reward. There is no bounce animation, no
RayCast2D, and no head-detection logic; both `body_entered` (walked into) and
`area_entered` for the `player_attack` group (punched) trigger
`_consume`.

---

## 9. Projectile (`projectile.gd`)

Type: `Area2D` on layer 5 (value 16), mask `1 + 2 = 3` (world + player). It
moves by mutating `position.x` directly in `_physics_process`, not by
`move_and_slide` — projectiles don't need to slide along surfaces. Two
despawn paths:

1. `LIFETIME = 3.0 s` countdown (handles the case where it sails off into
   sky/decoration that has no collider).
2. `body_entered` — damages the player (and frees) if it's the player; frees
   if it's a `TileMap` or `StaticBody2D`.

`SPEED = 110`, `DAMAGE = 1`. Direction is `±1` and is set by the firing
enemy via `p.set("direction", face_dir)` before `add_child`.

---

## 10. Animation system

The project uses **`AnimatedSprite2D`** everywhere (player, enemies,
projectile). There is **no `AnimationPlayer`** and no `AnimationTree` in the
project — past docs that reference them are stale.

`scripts/anim_util.gd` is a tiny helper:

```gdscript
static func build(specs: Array) -> SpriteFrames
```

Each `spec` is a dict `{name, path, count, w, h, fps, loop}`. The helper
loads the PNG once per spec, slices it into `count` `AtlasTexture` frames
(left to right, all the same size), and packs them into a `SpriteFrames`
resource. Every entity calls `AnimUtil.build(FRAMES_SPEC)` in `_ready` and
assigns the result to `sprite.sprite_frames`.

This is why every animation file in `assets/sprites/` is a single horizontal
strip and not an Aseprite/Godot import — the slicing happens in code.

---

## 11. Procedural level construction (`level_1.gd`)

The `level_1.tscn` scene is intentionally minimal: a `Node2D` root with a
script, an empty `TileMap`, an `Entities` container, and the BGM
`AudioStreamPlayer`. Everything else is built in code in `_ready()`:

1. **Tileset** — `_build_tileset()` constructs a `TileSet` from
   `assets/sprites/tiles/tileset.png`, declaring 11 atlas tiles and
   attaching a full-tile collision polygon to the three solid tiles
   (`A_BRICK`, `A_CONCRETE`, `A_ROOF`).
2. **Background layer (layer 0)** — `_paint_background()` paints sky, brick
   wall, decorative windows/doors, and diamond clouds across the full
   `WORLD_W = 100` × `WORLD_H = 9` grid.
3. **Foreground layer (layer 1)** — `_paint_foreground()` lays down the
   ground row plus six floating/rooftop platforms from the `PLATFORMS`
   table.
4. **Decor** — `_make_ladder()` builds three ladders (column + top/bottom
   rows) as `Area2D`s with sprites and a vertical rectangle shape.
   `_make_spike()` builds four hazard `Area2D`s.
5. **Pickups & enemies** — six question blocks, six foot soldiers, two
   shuriken throwers, one boss, all positioned by tile coordinates from the
   `PICKUPS` / `FOOT_SOLDIERS` / `SHURIKEN_THROWERS` / `BOSS_POS` tables.
6. **Player + camera** — `_spawn_player()` instantiates `player.tscn`,
   adds a `Camera2D` child with smoothing and limits set to the world
   bounds (`WORLD_W * TILE` × `WORLD_H * TILE`).
7. **HUD** — `_spawn_hud()` adds the HUD as a sibling, then
   `call_deferred("_attach_boss_to_hud", boss)` wires the boss bar one frame
   later.

There is **no `ParallaxBackground`** — the parallax-like effect comes from
the static painted background tiles being further "back" visually but
scrolling 1:1 with the camera.

To rebalance the level, edit the constants at the top of `level_1.gd`
(`PLATFORMS`, `LADDERS`, `SPIKES`, `PICKUPS`, `FOOT_SOLDIERS`,
`SHURIKEN_THROWERS`, `BOSS_POS`) — there is no level editor to learn.

---

## 12. HUD (`scenes/hud.tscn`, `hud.gd`)

Type: `CanvasLayer` so it draws on top of the level. Three pieces:

1. **Hearts row** — `Root/Top/Hearts` (`HBoxContainer`). Rebuilt from scratch
   on every `hp_changed` signal: `max_hp` `TextureRect`s using either
   `heart.png` (for indices `< current_hp`) or `heart_empty.png`.
2. **Lives + score labels** — `Root/Top/Stats/Lives` shows `xN`, score is
   formatted as `%05d`.
3. **Boss bar** — `Root/BossBar` is hidden until `attach_boss(boss)` is
   called by the level. The fill is a `ColorRect` whose `size.x` is set to
   `64.0 * (current / maximum)` on every `boss_hp_changed`. On
   `boss_defeated` the entire bar is hidden.

The HUD subscribes to **`GameManager.score_changed`,
`GameManager.lives_changed`, and `GameManager.hp_changed`** in `_ready` and
to the boss's two signals in `attach_boss`. It never reads scene-tree state
directly.

---

## 13. Asset pipeline (`tools/gen_assets.py`)

The script **generates every PNG and WAV from scratch** — it does not
process pre-existing art. Run it from the repo root:

```bash
python3 steel_streets/tools/gen_assets.py
```

It only depends on `Pillow` (image generation) and the Python stdlib `wave`
and `struct` modules (audio generation). Outputs go directly into
`steel_streets/assets/sprites/...` and `steel_streets/assets/audio/...`,
overwriting whatever's there.

### Sprite generation

- `img_from_rows(rows)` is the core primitive: rows are strings like
  `".0123."` where `.` is transparent and `0..3` are the four GB palette
  indices. The function returns an RGBA `Image`.
- Per-entity functions assemble those images into horizontal strips
  (`hcat`) — e.g. `gen_player()` produces `idle.png`, `walk.png`,
  `jump.png`, `attack.png`, `climb.png`, `hurt.png` as 2 / 4 / 2 / 3 / 2 / 1
  frame strips at 16×24 px each. The frame counts and dimensions match the
  `FRAMES_SPEC` arrays inside the GDScript files — **keep them in sync**.
- `gen_tileset()` produces an 11-tile horizontal strip used by
  `level_1.gd`.
- `render_text(text, color_idx, scale)` rasterises strings using a 5×7
  bitmap font defined inline at the bottom of the file. `gen_text_assets()`
  uses it to bake the title, "PRESS START", "PRESS ANY KEY", "GAME OVER",
  "LEVEL COMPLETE", "BOSS", and the splash caption as PNGs. There is no
  runtime font / `Theme` — all UI text is pre-rendered.

### Audio generation

- `square_tone(freq, dur, vol, attack, release)` produces a square wave
  envelope.
- `slide(f0, f1, dur, vol)` interpolates frequency for sweeps.
- `noise_burst(dur, vol, decay)` produces white-noise bursts for hits and
  game-over.
- `gen_audio()` composes those primitives into the seven WAV files
  (`jump`, `hit`, `attack`, `pickup`, `hurt`, `victory`, `game_over`, plus a
  short `bgm` loop).

### Import settings

All sprite imports (`*.png.import`) use the engine defaults; pixelation comes
from the global `rendering/textures/canvas_textures/default_texture_filter=0`
in `project.godot`, **not** from per-import filter overrides. Adding new
sprites does not require touching their `.import` files.

---

## 14. What does *not* exist (anti-knowledge)

These are common assumptions from older docs / generated wikis that are
**wrong** for this codebase. If you read them somewhere, treat them as bugs.

- ❌ There is no `AnimationPlayer` or `AnimationTree`.
- ❌ There is no `ParallaxBackground`.
- ❌ There is no `apply_palette()` or `generate_spritesheet()` function in
  `gen_assets.py` — the script generates from arrays of palette indices.
- ❌ There are no `player_hit`, `enemy_died`, or `health_changed` signals.
- ❌ There is no D-pad-specific input action — gamepad d-pad is bound to the
  same six actions everything else uses.
- ❌ The boss does not jump and does not summon minions.
- ❌ Question blocks do not spawn coins or power-ups; they heal and award
  score in place.
- ❌ Steel Streets is not used as a Godot engine smoke test.
