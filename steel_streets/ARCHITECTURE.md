# Steel Streets — Architecture Guide

Technical reference for developers working on the Steel Streets mini-game.
For gameplay overview, controls, and running instructions see [README.md](README.md).

---

## High-Level Architecture

```
┌──────────────────────────────────────────────────────────────────┐
│  GameManager (Autoload Singleton)                                │
│  scripts/game_manager.gd                                         │
│  Owns: lives, score, current_hp, max_hp                          │
│  Signals: score_changed, lives_changed, hp_changed               │
│  Navigation: goto_title / goto_splash / goto_level /             │
│              goto_game_over / goto_victory                        │
└──────────┬───────────────────────────────────────────────────────┘
           │ change_scene_to_file()
           ▼
┌─────────────────────────────────────────────────────────────────┐
│  Scene Flow                                                      │
│                                                                  │
│  title_screen ──▶ splash_screen ──▶ level_1 ──┬▶ victory        │
│       ▲                                        │                 │
│       └──────────── game_over ◀────────────────┘                 │
└─────────────────────────────────────────────────────────────────┘
```

### Scene Descriptions

| Scene              | Script              | Role                                              |
| ------------------ | ------------------- | ------------------------------------------------- |
| `title_screen`     | `title_screen.gd`   | Blinks "PRESS START", resets run via `GameManager.reset_run()` |
| `splash_screen`    | `splash_screen.gd`  | Character splash; auto-advances after 2.5 s or on input |
| `level_1`          | `level_1.gd`        | Procedurally built level (tilemap, entities, HUD)  |
| `game_over`        | `game_over.gd`      | Shown when lives reach 0; any input → title screen |
| `victory`          | `victory.gd`        | Shown after boss defeat; displays final score      |

---

## Entity Hierarchy

```
CharacterBody2D
├── Player             (scripts/player.gd)
│   ├── AnimatedSprite2D   ($Sprite)
│   ├── Area2D             ($AttackHitbox + $AttackHitbox/Shape)
│   ├── AudioStreamPlayer  ($SfxJump, $SfxAttack, $SfxHurt)
│   └── Camera2D           (added at runtime by level_1.gd)
│
└── EnemyBase          (scripts/enemy_base.gd)   ← abstract base
    ├── FootSoldier    (scripts/foot_soldier.gd)  ← melee grunt
    ├── ShurikenThrower(scripts/shuriken_thrower.gd) ← ranged
    └── Boss           (scripts/boss.gd)          ← end-of-level

Area2D
├── Projectile         (scripts/projectile.gd)    ← shuriken
└── QuestionBlock      (scripts/question_block.gd)← pickup
```

### Player (`player.gd`)

- Extends `CharacterBody2D`. States: idle, walk, jump, attack, climb, hurt, dead.
- Movement: `SPEED = 60`, `JUMP_VELOCITY = -190`, `GRAVITY = 700`.
- Attack: 0.30 s swing window + 0.15 s cooldown. Hitbox active during middle 75 % of swing.
- Ladder climbing: tracked via `ladders_overlapping` counter (enter/exit callbacks from level).
- Damage: knockback away from source, 1.2 s invincibility with 12 Hz sprite strobe.
- Death: plays hurt anim → `GameManager.take_life()` → respawn or game-over after delay.

### EnemyBase (`enemy_base.gd`)

- Shared HP, contact damage, score value, hit-flash (0.18 s white strobe at 20 Hz).
- Hurtbox (Area2D) detects `player_attack` group → `take_damage()`.
- Hitbox (Area2D) detects player body → `body.take_damage(contact_damage, global_position)`.
- On death: awards score, disables all collision, `queue_free()`.

### FootSoldier (`foot_soldier.gd`)

- HP: 2, contact damage: 1, score: 100.
- Patrols at 22 px/s; chases player at 35 px/s within 70 px range.
- Turns at walls or ledge edges (raycast floor-check ahead).

### ShurikenThrower (`shuriken_thrower.gd`)

- HP: 3, contact damage: 1, score: 150.
- Paces ±14 px around spawn. Fires shuriken every 1.6 s when player is within 110 px.
- Shuriken: `Projectile` (Area2D), speed 110 px/s, 3 s lifetime, 1 damage.

### Boss (`boss.gd`)

- HP: 9, contact damage: 1, score: 1000.
- State machine: `PAUSE(1 s) → WINDUP(0.5 s) → CHARGE(1 s @ 70 px/s) → RECOVER(0.7 s)`.
- During windup, steps backwards slightly and brightens sprite.
- Emits `boss_hp_changed` / `boss_defeated` signals (consumed by HUD and level).

---

## Procedural Level Generation (`level_1.gd`)

The level is built entirely in code — no hand-authored tilemap data in the `.tscn`.

### World Parameters
- **Grid**: 100 × 9 tiles, each tile 16 × 16 px → world is 1600 × 144 px.
- **Ground**: full-width brick row at y = 8.
- **Platforms**: 5 elevated platforms (concrete, roof) defined in `PLATFORMS` array.
- **Ladders**: 3 ladders at columns 27, 53, 69 — each an `Area2D` with collision shape + sprites.
- **Spikes**: 4 spike hazards (1 damage on contact).
- **Pickups**: 6 question blocks (50 score + 1 HP heal).

### Tileset Construction
The tileset is built programmatically from a single 11-tile-wide atlas strip (`tileset.png`).
Solid tiles (`A_BRICK`, `A_CONCRETE`, `A_ROOF`) get a full-tile collision polygon.
Background tiles (sky, brick wall, windows, doors, diamonds) are painted on layer 0;
foreground/solid tiles go on layer 1.

### Spawn Order
`_ready()` → build tileset → paint BG → paint FG → spawn decor (ladders, spikes) →
spawn pickups → spawn enemies → spawn player (+ camera) → spawn boss → spawn HUD → play BGM.

### Camera
A `Camera2D` is attached to the player at runtime with position smoothing (speed 8).
Limits: `left=0`, `top=0`, `right=1600`, `bottom=144`.

---

## Animation System (`anim_util.gd`)

A static helper that builds `SpriteFrames` from horizontal sprite-sheet strips.
Each entity declares a `FRAMES_SPEC` array of dictionaries:

```gdscript
{"name": "walk", "path": "res://...", "count": 4, "w": 16, "h": 24, "fps": 8.0, "loop": true}
```

`AnimUtil.build(specs)` creates one `SpriteFrames` resource with an animation per spec,
slicing the source texture into `AtlasTexture` frames.

---

## HUD (`hud.gd`)

- Hearts row: rebuilt on every `hp_changed` signal (TextureRect per HP point).
- Lives counter: `"x%d"` format.
- Score: 5-digit zero-padded.
- Boss bar: hidden by default; shown when `attach_boss()` is called.
  Bar width = `64 × (current_hp / max_hp)` pixels.

---

## Signal Flow

```
GameManager.score_changed ──────────▶ HUD._on_score_changed
GameManager.lives_changed ──────────▶ HUD._on_lives_changed
GameManager.hp_changed ─────────────▶ HUD._on_hp_changed

Boss.boss_hp_changed ───────────────▶ HUD._on_boss_hp_changed
Boss.boss_defeated ─────────────────▶ HUD._on_boss_defeated
Boss.boss_defeated ─────────────────▶ Level1._on_boss_defeated
                                       └──▶ GameManager.goto_victory()
```

---

## Input Map (from `project.godot`)

| Action       | Keys                     | Gamepad           |
| ------------ | ------------------------ | ----------------- |
| `move_left`  | `←` / `A`               | left stick / dpad |
| `move_right` | `→` / `D`               | left stick / dpad |
| `jump`       | `Space` / `W`            | A button          |
| `attack`     | `Z` / `J`               | X button          |
| `climb_up`   | `↑` / `W`               | dpad up           |
| `climb_down` | `↓` / `S`               | dpad down         |

---

## Asset Pipeline (`tools/gen_assets.py`)

All sprites and audio are generated programmatically.

```bash
pip install Pillow
python3 tools/gen_assets.py
```

This regenerates every PNG and WAV from code, ensuring the art/sound pipeline is
fully reproducible without binary asset dependencies. Output directories:
- `assets/sprites/` — player, enemies, tiles, items, projectiles, UI
- `assets/audio/` — chiptune SFX + BGM loop

---

## Display Configuration

| Setting              | Value     |
| -------------------- | --------- |
| Viewport             | 160 × 144 |
| Window override      | 640 × 576 (4× scale) |
| Stretch mode         | `viewport` (nearest-neighbor) |
| Stretch aspect       | `keep`    |
| Renderer             | GL Compatibility |

The 160 × 144 resolution and 4-color green palette recreate the original Game Boy display.

---

## Collision Layers

| Layer | Bit | Usage                  |
| ----- | --- | ---------------------- |
| 1     | 1   | World / tilemap solids |
| 2     | 2   | Player body            |
| 6     | 32  | Ladders                |
| 8     | 128 | Hazards (spikes)       |

---

## Game Constants Quick Reference

| Constant         | Value   | Location            |
| ---------------- | ------- | ------------------- |
| Starting lives   | 3       | `game_manager.gd`   |
| Starting HP      | 5       | `game_manager.gd`   |
| Invincibility    | 1.2 s   | `player.gd`         |
| Player speed     | 60 px/s | `player.gd`         |
| Jump velocity    | -190    | `player.gd`         |
| Attack duration  | 0.30 s  | `player.gd`         |
| Attack cooldown  | 0.15 s  | `player.gd`         |
| Boss HP          | 9       | `boss.gd`           |
| Boss charge speed| 70 px/s | `boss.gd`           |
| Shuriken speed   | 110 px/s| `projectile.gd`     |
| Shuriken lifetime| 3.0 s   | `projectile.gd`     |
| Pickup score     | 50      | `question_block.gd` |
| Pickup heal      | 1 HP    | `question_block.gd` |

---

## Adding Content

### New Enemy Type
1. Create a new script extending `enemy_base.gd`.
2. Set `max_hp`, `contact_damage`, `score_value` in `_ready()` before calling `super()`.
3. Create a `.tscn` with `AnimatedSprite2D` ($Sprite), `Area2D` ($Hurtbox + $Hitbox), and
   `AudioStreamPlayer` ($SfxHit). Use the same node names as existing enemies.
4. Add a `FRAMES_SPEC` and call `AnimUtil.build()` in `_ready()`.
5. Add spawn entries in `level_1.gd` (column, row) and preload the scene.

### New Level
1. Duplicate `level_1.gd` and `level_1.tscn`.
2. Modify `PLATFORMS`, `LADDERS`, `SPIKES`, `PICKUPS`, and enemy arrays.
3. Add a scene constant in `game_manager.gd` and a `goto_level_N()` helper.
4. Wire up level transitions (e.g., boss defeat → next level or victory).
