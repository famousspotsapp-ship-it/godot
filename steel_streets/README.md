# Steel Streets: Searching for Master Plank!

A side-scrolling beat-'em-up platformer targeting Godot 4.3+ that channels the
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
   When using the editor built from this repository, run:
   ```bash
   ./bin/godot.linuxbsd.editor.dev.x86_64 --path steel_streets
   ```
   This uses the 4.7-beta editor built from this repository.

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

## Architecture

### Scene flow

The configured entry point is `scenes/title_screen.tscn`
(`application/run/main_scene` in `project.godot`). The runtime flow is:
Because the project entry point is the title scene, the code flow starts with
title and then shows the splash; there is no splash-to-title transition.

1. **Title → splash:** `title_screen.gd::_advance()` calls
   `GameManager.goto_splash()`. The autoload's `goto_splash()` calls
   `get_tree().change_scene_to_file(SPLASH_SCENE)`.
   `title_screen.gd::_ready()` calls `GameManager.reset_run()` before the
   title screen accepts input.
2. **Splash → level:** `splash_screen.gd::_ready()` waits 2.5 seconds and
   calls `GameManager.goto_level()`; `_skip()` does the same for key, joypad,
   or mouse input. `goto_level()` calls
   `get_tree().change_scene_to_file(LEVEL_SCENE)`.
3. **Level → victory:** `level_1.gd::_spawn_boss()` connects the boss's
   `boss_defeated` signal to `_on_boss_defeated()`. After a 1.5-second delay,
   `_on_boss_defeated()` calls `GameManager.goto_victory()`, which changes to
   `VICTORY_SCENE` with `get_tree().change_scene_to_file(...)`.
4. **Level → game over or retry:** `player.gd::_die()` calls
   `GameManager.take_life()`. If it returns true, the player waits one second
   and calls `GameManager.goto_level()` to reload the level; otherwise it
   waits 1.2 seconds and calls `GameManager.goto_game_over()`.
   `goto_game_over()` changes to `GAME_OVER_SCENE`.
5. **Victory/game over → title:** `victory.gd::_restart()` and
   `game_over.gd::_restart()` call `GameManager.goto_title()`, whose
   `get_tree().change_scene_to_file(TITLE_SCENE)` returns to the title and
   starts a new run.

### GameManager autoload

`project.godot` registers `scripts/game_manager.gd` as the `GameManager`
autoload. It persists run state across scene changes and centralizes
navigation.

**State variables**

| Name | Initial/default value | Purpose |
| ---- | -------------------- | ------- |
| `lives` | `STARTING_LIVES` (`3`) | Remaining player lives. |
| `score` | `0` | Current run score. |
| `max_hp` | `STARTING_HP` (`5`) | Maximum HP for the current life. |
| `current_hp` | `STARTING_HP` (`5`) | Current player HP. |

**Signals**

| Signal | Arguments | Emitted when |
| ------ | --------- | ------------ |
| `score_changed` | `new_score: int` | `score` changes. |
| `lives_changed` | `new_lives: int` | `lives` changes. |
| `hp_changed` | `current: int, maximum: int` | `current_hp` or its maximum is updated. |

**Public methods**

| Method | Description |
| ------ | ----------- |
| `reset_run()` | Restores `lives`, `score`, `max_hp`, and `current_hp` to their starting values and emits all three state signals. |
| `add_score(amount)` | Adds `amount` to `score` and emits `score_changed`. |
| `set_hp(value)` | Clamps `value` to `0..max_hp`, stores it in `current_hp`, and emits `hp_changed`. |
| `heal(amount)` | Increases HP by calling `set_hp(current_hp + amount)`. |
| `take_life()` | Decrements `lives`; returns `true` and restores HP when lives remain, otherwise returns `false`. |
| `goto_title()` | Changes to `TITLE_SCENE`. |
| `goto_splash()` | Changes to `SPLASH_SCENE`. |
| `goto_level()` | Changes to `LEVEL_SCENE`. |
| `goto_game_over()` | Changes to `GAME_OVER_SCENE`. |
| `goto_victory()` | Changes to `VICTORY_SCENE`. |

All five navigation methods call `get_tree().change_scene_to_file(...)` with
the corresponding scene constant.

### Script responsibilities

| File | Attached scene | Base class (`extends`) | Responsibility |
| ---- | -------------- | ---------------------- | -------------- |
| `scripts/anim_util.gd` | None; preloaded helper | `Object` | Builds `SpriteFrames` from horizontal sprite-sheet specifications. |
| `scripts/boss.gd` | `scenes/boss.tscn` | `res://scripts/enemy_base.gd` | Runs the boss pause/wind-up/charge/recover state machine, emits HP and defeat signals, and awards the boss score on death. |
| `scripts/enemy_base.gd` | None; base for enemy scenes | `CharacterBody2D` | Provides shared enemy HP, contact damage, attack-area handling, hit flash, score award, hitbox shutdown, and freeing. |
| `scripts/foot_soldier.gd` | `scenes/foot_soldier.tscn` | `res://scripts/enemy_base.gd` | Implements patrol, ledge/wall turning, and short-range player chasing for melee soldiers. |
| `scripts/game_manager.gd` | Autoload `GameManager`; no scene attachment | `Node` | Owns persistent run state and all scene-transition helpers. |
| `scripts/game_over.gd` | `scenes/game_over.tscn` | `Control` | Returns to the title when a key, joypad button, or mouse button is pressed. |
| `scripts/hud.gd` | `scenes/hud.tscn` | `CanvasLayer` | Displays hearts, lives, and score, and shows/updates the boss health bar. |
| `scripts/level_1.gd` | `scenes/level_1.tscn` | `Node2D` | Builds the tileset and procedural level, spawns entities/pickups/HUD, creates ladders and hazards, follows the player with a camera, and routes boss defeat to victory. |
| `scripts/player.gd` | `scenes/player.tscn` | `CharacterBody2D` | Handles movement, jumping, melee attack hitbox timing, ladder climbing, animation, damage, invincibility, lives, and respawn/game-over flow. |
| `scripts/projectile.gd` | `scenes/projectile.tscn` | `Area2D` | Moves enemy shurikens, damages the player on contact, and frees them on world collision or after their lifetime. |
| `scripts/question_block.gd` | `scenes/question_block.tscn` | `Area2D` | Consumes on player contact or player attack, awards score, optionally heals, plays pickup audio, and frees itself. |
| `scripts/shuriken_thrower.gd` | `scenes/shuriken_thrower.tscn` | `res://scripts/enemy_base.gd` | Paces around its spawn point and fires projectile scenes toward a nearby player. |
| `scripts/splash_screen.gd` | `scenes/splash_screen.tscn` | `Control` | Advances automatically after `HOLD_TIME` or immediately on common input. |
| `scripts/title_screen.gd` | `scenes/title_screen.tscn` | `Control` | Resets the run, blinks the prompt, and advances to the splash screen on input. |
| `scripts/victory.gd` | `scenes/victory.tscn` | `Control` | Displays the final `GameManager.score` and returns to the title on input. |

`boss.gd`, `foot_soldier.gd`, and `shuriken_thrower.gd` inherit from
`enemy_base.gd`; the base script itself is not attached directly to a scene.

### Physics layers & collision

`project.godot` names all eight 2D physics layers. Collision values in the
entity scenes are bitmasks: layer 1 is `1`, layer 2 is `2`, layer 3 is `4`,
and so on.

| Layer | `project.godot` name | Nodes using the layer or mask |
| ----- | -------------------- | ----------------------------- |
| 1 | `world` | The procedural `TileMap` sets its TileSet physics layer to collision layer `1` in `level_1.gd`; `Player`, all enemy `CharacterBody2D` nodes, and `Projectile` use it in their collision masks (`1` or `3`). |
| 2 | `player` | `Player` is layer `2`; enemy `Hitbox` areas, `Projectile`, procedural ladder/hazard areas, and `QuestionBlock` include it in their masks (`2`, `3`, or `6`). |
| 3 | `player_hitbox` | `Player/AttackHitbox` is layer `4` with mask `72` (enemy + pickup); enemy `Hurtbox` areas mask `4`, and `QuestionBlock` masks `6` (player + player hitbox). |
| 4 | `enemy` | Enemy `CharacterBody2D` roots and their `Hurtbox` areas are layer `8`; `Player/AttackHitbox` includes it through mask `72`. |
| 5 | `enemy_hitbox` | Enemy `Hitbox` areas and `Projectile` are layer `16`; enemy hitboxes mask player (`2`), while `Projectile` masks world and player (`3`). |
| 6 | `ladder` | Procedural ladder `Area2D` nodes are layer `32` and mask player (`2`). |
| 7 | `pickup` | `QuestionBlock` is layer `64`; `Player/AttackHitbox` includes it through mask `72`. |
| 8 | `hazard` | Procedural spike `Area2D` nodes are layer `128` and mask player (`2`). |

The repeated enemy scene setup is the same in `boss.tscn`,
`foot_soldier.tscn`, and `shuriken_thrower.tscn`: the root body is layer `8`
and masks world (`1`), `Hurtbox` is layer `8` and masks player hitboxes (`4`),
and `Hitbox` is layer `16` and masks the player (`2`). `player.tscn`,
`projectile.tscn`, and `question_block.tscn` contain the other scene-authored
values; ladders, spikes, and the TileMap physics layer are created by
`level_1.gd`.

### Input actions

The seven actions defined in `project.godot` are `move_left`, `move_right`,
`jump`, `attack`, `climb_up`, `climb_down`, and `start`. `player.gd` reads
the first six with `Input.get_axis`, `Input.is_action_just_pressed`, and
`Input.is_action_pressed`; the title, splash, victory, and game-over scripts
advance on raw key, joypad-button, or mouse-button events rather than reading
the `start` action directly.

### Rendering setup

The project settings configure a Game Boy-sized render target and nearest-pixel
presentation:

- `display/window/size/viewport_width=160` and
  `display/window/size/viewport_height=144` set the internal viewport.
- `display/window/stretch/mode="viewport"` scales that viewport
  (`display/window/stretch/aspect="keep"` preserves its aspect ratio).
- `application/config/features=PackedStringArray("4.3", "GL Compatibility")`
  declares the Godot 4.3 feature target; the renderer is explicitly
  `renderer/rendering_method="gl_compatibility"` (and the mobile variant has
  the same value).
- `textures/canvas_textures/default_texture_filter=0` selects nearest
  filtering.
- `2d/snap/snap_2d_transforms_to_pixel=true` and
  `2d/snap/snap_2d_vertices_to_pixel=true` enable 2D pixel snapping.

### Asset generation

`tools/gen_assets.py` uses Pillow for RGBA PNGs and Python's `wave` module for
mono 22,050 Hz WAVs. Its four-color `GB` palette is `#e0f8d0`, `#88c070`,
`#346856`, and `#081820` (plus transparent pixels).

It emits these sprite sheets and standalone images:

- Player sheets in `assets/sprites/player/`: `idle.png`, `walk.png`,
  `jump.png`, `attack.png`, `climb.png`, and `hurt.png`.
- Enemy sheets in `assets/sprites/enemies/`: `foot_soldier.png`,
  `shuriken_thrower.png`, and `boss.png`; the projectile sheet is
  `assets/sprites/projectiles/shuriken.png`.
- `assets/sprites/tiles/tileset.png`, plus standalone
  `assets/sprites/items/ladder.png`, `question.png`, and `spike.png`.
- UI images in `assets/sprites/ui/`: `heart.png`, `heart_empty.png`,
  `portrait.png`, `title_main.png`, `title_sub_a.png`, `title_sub_b.png`,
  `press_start.png`, `press_any_key.png`, `game_over.png`, `victory.png`,
  `boss_label.png`, and `splash_caption.png`.

It emits `assets/audio/jump.wav`, `attack.wav`, `hit.wav`, `hurt.wav`,
`pickup.wav`, `victory.wav`, `game_over.wav`, and `bgm.wav`. From the
repository root, regenerate everything with:

```bash
python3 steel_streets/tools/gen_assets.py
```

The script creates parent directories as needed and requires Pillow
(`pip install Pillow`).

## Gameplay

- 3 lives, 5 HP per life. Damage triggers ~1.2 s of invincibility frames.
- Defeat foot soldiers, ranged shuriken throwers, and the boss to score
  points and reach the victory screen.
- Punch question blocks (or walk into them) for score and a small heal.
- A 5–6-screen-wide level with rooftop platforming and ladders to climb.

Beating the boss takes the player to the victory screen; running out of
lives takes them to the game-over screen, both of which return to the
title on any input.
