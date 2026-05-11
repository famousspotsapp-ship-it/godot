# Steel Streets: Searching for Master Plank!

A side-scrolling beat-'em-up platformer built in Godot 4.3 that channels the
look and feel of the original Game Boy *Teenage Mutant Ninja Turtles* games
(*Fall of the Foot Clan*, *Back from the Sewers*).

The whole game renders into a 160x144 internal viewport scaled up with
nearest-neighbor filtering and is restricted to the classic Game Boy
green palette:

| Light    | Lite     | Dark     | Black    |
| -------- | -------- | -------- | -------- |
| `#e0f8d0` | `#88c070` | `#346856` | `#081820` |

## Running

1. Install [Godot 4.3+](https://godotengine.org/download).
2. Open the editor and import this folder, or run from the command line:
   ```bash
   godot --path steel_streets
   ```
   The main scene (`scenes/title_screen.tscn`) launches automatically.

## Controls

| Action | Keyboard | Gamepad |
| ------ | -------- | ------- |
| Move left / right | `A` / `D` or `←` / `→` | left stick / d-pad |
| Jump | `Space` or `W` | A button |
| Attack (melee) | `Z` or `J` | X button |
| Climb up / down (on ladder) | `W` / `S` or `↑` / `↓` | d-pad |
| Start / advance screen | `Enter` or `Space` | Start |

## Project layout

```
steel_streets/
├── project.godot          # input map, autoload, viewport scaling
├── icon.svg
├── scenes/                # title, splash, level, game over, victory + entity scenes
├── scripts/               # GDScript files (autoload, player, enemies, HUD, ...)
├── assets/
│   ├── sprites/           # 4-color GB pixel art (player, enemies, tiles, UI)
│   └── audio/             # chiptune-style WAV SFX + a short BGM loop
└── tools/
    └── gen_assets.py      # regenerates every PNG/WAV from code (Pillow + wave)
```

All sprites and audio are generated programmatically by
`tools/gen_assets.py` so the entire art and sound pipeline is reproducible:

```bash
python3 steel_streets/tools/gen_assets.py
```

(Requires Pillow: `pip install Pillow`.)

The script doesn't process external textures — every sprite is painted from
hand-authored ASCII grids (`.` for transparent, `0`–`3` for the four GB
palette indices) using Pillow, and every sound effect is synthesised
frame-by-frame and written with the stdlib `wave` module. Re-running the
script is the canonical way to refresh art and audio.

## Gameplay

- 3 lives, 5 HP per life. Damage triggers ~1.2 s of invincibility frames.
- Defeat foot soldiers, ranged shuriken throwers, and the boss to score
  points and reach the victory screen.
- Punch question blocks (or walk into them) for score and a small heal.
- A 5–6-screen-wide level with rooftop platforming and ladders to climb.

Beating the boss takes the player to the victory screen; running out of
lives takes them to the game-over screen, both of which return to the
title on any input.

## Scene flow

All scene transitions are routed through the `GameManager` autoload
(`scripts/game_manager.gd`):

```
title_screen.tscn  ── any key ──▶  splash_screen.tscn
                                          │
                                          ▼
                                    level_1.tscn
                              ╱                  ╲
              boss defeated  ╱                    ╲  lives ≤ 0
                            ▼                      ▼
                    victory.tscn            game_over.tscn
                            ╲                      ╱
                             ╲                    ╱
                              ▶ title_screen.tscn  ◀
```

## Technical reference

This section documents the facts the rest of the team and any
documentation generator (e.g. DeepWiki) should rely on. The numbers and
names below are sourced directly from `project.godot` and the scripts in
`scripts/`.

### Display & rendering

| Setting | Value | Source |
| --- | --- | --- |
| Internal viewport | 160 × 144 | `display/window/size/viewport_{width,height}` |
| Window default | 640 × 576 (4×) | `display/window/size/window_{width,height}_override` |
| Stretch mode / aspect | `viewport` / `keep` | `display/window/stretch/{mode,aspect}` |
| Renderer | `gl_compatibility` (desktop & mobile) | `rendering/renderer/rendering_method[.mobile]` |
| Default texture filter | Nearest (0) | `rendering/textures/canvas_textures/default_texture_filter` |
| 2D snap | transforms + vertices snap to pixel | `rendering/2d/snap/snap_2d_{transforms,vertices}_to_pixel` |
| Engine features | `["4.3", "GL Compatibility"]` | `application/config/features` |

### Input actions

All seven actions are defined in `project.godot` and resolved by
`Input.get_axis("move_left", "move_right")` / `Input.is_action_pressed(...)`
in `scripts/player.gd` and the UI scripts.

| Action | Keyboard | Gamepad |
| --- | --- | --- |
| `move_left` | `A`, `←` | left-stick X<0, d-pad left (button 13) |
| `move_right` | `D`, `→` | left-stick X>0, d-pad right (button 14) |
| `jump` | `Space`, `W` | A button (0) |
| `attack` | `Z`, `J` | X button (2) |
| `climb_up` | `W`, `↑` | d-pad up (button 11) |
| `climb_down` | `S`, `↓` | d-pad down (button 12) |
| `start` | `Enter`, `Space` | Start button (7) |

### Physics / collision layers

`project.godot` names eight 2D physics layers. Bodies and areas in the
scenes use the indices below (1-based to match the engine inspector).

| # | Name | Used by |
| - | --- | --- |
| 1 | `world` | TileMap and static geometry built in `level_1.gd` |
| 2 | `player` | `Player.collision_layer` (`scenes/player.tscn`) |
| 3 | `player_hitbox` | `Player/AttackHitbox.collision_layer` |
| 4 | `enemy` | `enemy_base.gd`-derived `CharacterBody2D`s |
| 5 | `enemy_hitbox` | Shuriken `Area2D` and enemy contact hitboxes |
| 6 | `ladder` | `Area2D` ladders spawned by `level_1.gd` |
| 7 | `pickup` | `question_block.gd` |
| 8 | `hazard` | Spike `Area2D`s spawned by `level_1.gd` |

### Autoload: `GameManager`

Registered as `GameManager="*res://scripts/game_manager.gd"`. It owns
run state and centralises scene transitions.

State:

| Field | Type | Initial |
| --- | --- | --- |
| `lives` | `int` | 3 (`STARTING_LIVES`) |
| `score` | `int` | 0 |
| `max_hp` | `int` | 5 (`STARTING_HP`) |
| `current_hp` | `int` | 5 |

Signals:

- `score_changed(new_score: int)`
- `lives_changed(new_lives: int)`
- `hp_changed(current: int, maximum: int)`

Key methods: `reset_run()`, `add_score(amount)`, `set_hp(value)`,
`heal(amount)`, `take_life()` (returns `false` when out of lives),
and the navigation helpers `goto_title()`, `goto_splash()`,
`goto_level()`, `goto_game_over()`, `goto_victory()`.

### Scripts at a glance

| Script | Role |
| --- | --- |
| `scripts/game_manager.gd` | Autoload — lives/score/HP + scene navigation |
| `scripts/anim_util.gd` | Helper that turns horizontal sprite strips into `SpriteFrames` resources used by every animated entity |
| `scripts/player.gd` | `CharacterBody2D` player: walking, jumping, melee attack via `$AttackHitbox` (Area2D, timer-driven — *not* an `AnimationPlayer`), ladder climbing, knockback, invincibility flash |
| `scripts/enemy_base.gd` | Shared `CharacterBody2D` base: HP, hit-flash, contact damage, death + score award |
| `scripts/foot_soldier.gd` | Patrols a platform, switches to chase when player is within `CHASE_RANGE` (70 px) |
| `scripts/shuriken_thrower.gd` | Paces ±14 px around its spawn, fires straight-horizontal `projectile.tscn` shurikens every 1.6 s |
| `scripts/boss.gd` | 4-state machine `PAUSE → WINDUP → CHARGE → RECOVER`, emits `boss_hp_changed` / `boss_defeated` for the HUD |
| `scripts/projectile.gd` | `Area2D` shuriken; flies in `direction`, despawns on world or after `LIFETIME` |
| `scripts/question_block.gd` | Pickup `Area2D`; +50 score and +1 HP when consumed (walked-into or attacked) |
| `scripts/level_1.gd` | Builds the level procedurally: `TileSet`, foreground/background tilemaps, ladders, spikes, pickups, enemies, boss, player and camera — there is *no* `ParallaxBackground` |
| `scripts/hud.gd` | `CanvasLayer` HUD: hearts, lives, score, optional boss bar; subscribes to `GameManager` signals |
| `scripts/title_screen.gd` / `splash_screen.gd` / `game_over.gd` / `victory.gd` | UI screens that route through `GameManager.goto_*()` |

### Asset pipeline

`tools/gen_assets.py` writes every file in `assets/sprites/**` and
`assets/audio/**` from scratch on each run:

- **Sprites:** Pillow renders 16×16 / 16×24 / 24×32 / 8×8 frames from
  hand-authored ASCII grids using the 4-colour GB palette
  (`#e0f8d0`, `#88c070`, `#346856`, `#081820`). Frames are concatenated
  horizontally into per-animation strips (`idle.png`, `walk.png`, ...).
- **Audio:** Square / triangle / noise waveforms are synthesised
  sample-by-sample (signed 16-bit, 22 050 Hz mono) and written with the
  stdlib `wave` module.
- **Godot import:** `*.png.import` files set `compress/mode=0`
  (lossless) and `filter=false`, so the pixel art is preserved
  end-to-end.

### Engine compatibility note

The project declares `config/features=PackedStringArray("4.3", "GL
Compatibility")` in `project.godot` and uses only stable Godot 4 APIs
(`CharacterBody2D`, `AnimatedSprite2D`, `Area2D`, `TileMap`, `Input`,
autoloads). It opens and runs against the Godot binary built from this
repository (currently 4.7-beta per `version.py`) as well as any
official 4.3+ download.
