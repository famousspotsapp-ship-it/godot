# Steel Streets: Searching for Master Plank!

A side-scrolling beat-'em-up platformer written in GDScript that channels the
look and feel of the original Game Boy *Teenage Mutant Ninja Turtles* games
(*Fall of the Foot Clan*, *Back from the Sewers*).

The project is bundled inside this Godot Engine checkout (currently
`4.7-beta`, see `version.py`) and runs with any Godot 4.x editor from 4.3
onwards; `project.godot` declares `config/features = ("4.3", "GL
Compatibility")` as its minimum.

The whole game renders into a 160x144 internal viewport scaled up with
nearest-neighbor filtering and is restricted to the classic Game Boy
green palette:

| Light    | Lite     | Dark     | Black    |
| -------- | -------- | -------- | -------- |
| `#e0f8d0` | `#88c070` | `#346856` | `#081820` |

## Running

1. Install [Godot 4.3+](https://godotengine.org/download), or build the editor
   from this repository (`scons target=editor dev_build=yes`).
2. Open the editor and import this folder, or run from the command line:
   ```bash
   godot --path steel_streets
   # or, with a local dev build:
   ./bin/godot.linuxbsd.editor.dev.x86_64 --path steel_streets
   ```
   The main scene (`scenes/title_screen.tscn`) launches automatically.

The game has no automated tests; it is verified by playing it. The engine's
own test suite (`--headless --test`) does not cover this folder.

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
python3 tools/gen_assets.py
```

(Requires Pillow: `pip install Pillow`.)

## Gameplay

- 3 lives, 5 HP per life. Damage triggers ~1.2 s of invincibility frames.
- Defeat foot soldiers, ranged shuriken throwers, and the boss to score
  points and reach the victory screen.
- Punch question blocks (or walk into them) for score and a small heal.
- A 5–6-screen-wide level with rooftop platforming and ladders to climb.

Beating the boss takes the player to the victory screen; running out of
lives takes them to the game-over screen, both of which return to the
title on any input.

## Architecture

### Scene flow

`title_screen` → `splash_screen` (auto-advances after 2.5 s, or on any input)
→ `level_1` → `victory` (boss defeated) or `game_over` (out of lives) → back to
`title_screen` on any input. Every transition goes through one of the
`GameManager.goto_*()` helpers; nothing else calls `change_scene_to_file()`.
The title screen calls `GameManager.reset_run()`, so run state is reset there
rather than on game over.

### GameManager (autoload)

`scripts/game_manager.gd` is the only autoload (`GameManager`) and owns all
state that survives a scene change:

| Member | Purpose |
| ------ | ------- |
| `lives`, `score`, `current_hp`, `max_hp` | Persistent run state (3 lives, 5 HP). |
| `score_changed`, `lives_changed`, `hp_changed` | Signals the HUD connects to. |
| `add_score()`, `set_hp()`, `heal()` | Mutators that emit the matching signal. |
| `take_life()` | Decrements lives, refills HP; returns `false` when the run is over. |
| `goto_title/splash/level/game_over/victory()` | Scene transitions. |

`scripts/hud.gd` (a `CanvasLayer`) rebuilds hearts/lives/score from those
signals, and `level_1.gd` calls `hud.attach_boss(boss)` to wire the boss bar to
the boss's `boss_hp_changed` / `boss_defeated` signals.

### Animation and combat conventions

The project deliberately uses **no** `AnimationPlayer`, `AnimationTree`,
shaders, or particle nodes. Instead:

- Every animated entity is an `AnimatedSprite2D` whose `SpriteFrames` are built
  at runtime by `AnimUtil.build()` (`scripts/anim_util.gd`) from a
  `FRAMES_SPEC` constant: horizontal PNG strips sliced into `AtlasTexture`
  frames with per-animation fps and loop flags.
- The player's attack hitbox is driven by plain timers, not animation tracks:
  `_update_attack_state()` enables `$AttackHitbox` only during the middle of
  the `ATTACK_TIME` window and offsets it 10 px in the facing direction.
- Damage feedback is code-driven: enemies strobe `sprite.modulate` for 0.18 s
  after a hit, and the player strobes `sprite.visible` at 12 Hz for the 1.2 s
  of invincibility.
- Enemies extend the base script by path (`extends
  "res://scripts/enemy_base.gd"`; there is no `class_name`), set `max_hp`,
  `contact_damage`, and `score_value` in `_ready()` before calling `super()`,
  and inherit health, contact damage, hit flash, and the death handshake
  (`GameManager.add_score()` then `queue_free()`).
- Enemy behavior is a hand-rolled state machine: `foot_soldier` patrols and
  chases within 70 px, `shuriken_thrower` paces and fires
  `scenes/projectile.tscn` every 1.6 s within 110 px, and `boss` cycles
  `PAUSE → WINDUP → CHARGE → RECOVER` with 9 HP.
- Cross-entity damage is duck-typed through groups: hitboxes check
  `is_in_group("player")` / `"player_attack"` / `"enemy"` / `"boss"` and call
  `take_damage()` if the node has the method. The player's signature is
  `take_damage(amount: int, from_position: Vector2)` so knockback can be
  derived.

### Level construction

`scripts/level_1.gd` builds the whole level in code — `TileSet`, the background
and foreground layers of a single `TileMap` node, ladders, spikes, question
blocks, enemies, the boss, the player, and a world-bounded `Camera2D` — from
the `PLATFORMS`, `LADDERS`, `SPIKES`, and `PICKUPS` constants at the top of the
file. There is no hand-authored tilemap scene, so level edits happen in that
script.

Note: the level and `scripts/projectile.gd` still use `TileMap`, which is
deprecated in Godot 4.x in favor of `TileMapLayer`. It works in 4.7-beta but
is the most likely source of future breakage when the engine drops it.

### Physics layers

Layer names live in `project.godot` under `[layer_names]`:

| Bit | Layer | Used by |
| --- | ----- | ------- |
| 1 | `world` | Tilemap collision; masked by every character body. |
| 2 | `player` | Player body. |
| 3 | `player_hitbox` | Player attack `Area2D` (group `player_attack`). |
| 4 | `enemy` | Enemy bodies and hurtboxes. |
| 5 | `enemy_hitbox` | Enemy contact hitboxes and projectiles. |
| 6 | `ladder` | Ladder `Area2D`s that drive climbing. |
| 7 | `pickup` | Question blocks. |
| 8 | `hazard` | Spikes. |

### Rendering settings that matter

`gl_compatibility` renderer, `default_texture_filter=0` (nearest), 2D transform
and vertex pixel snapping on, `viewport` stretch mode with `keep` aspect, and a
640×576 window override so the 160×144 viewport scales 4×. Changing any of
these breaks the pixel-perfect look.
