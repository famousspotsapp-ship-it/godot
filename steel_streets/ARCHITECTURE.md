# Steel Streets — Architecture

Code-level reference for the bundled `steel_streets/` game project. Everything
below is read from the scripts in `scripts/` and from `project.godot`; use it
instead of inferring behaviour from engine-side classes.

## Runtime shape

- Pure GDScript + `.tscn` project (~1200 lines across 15 scripts). No C++,
  no GDExtension, no custom engine module: the project only consumes the
  engine in this repository.
- Single autoload: `GameManager` (`scripts/game_manager.gd`).
- Main scene is `scenes/title_screen.tscn` (not the splash screen).
- 160x144 viewport, `stretch/mode="viewport"`, `aspect="keep"`, nearest
  filtering, 2D pixel snapping, `gl_compatibility` renderer.
- `config/features` still declares `4.3`, while the engine in this repo is
  4.7-beta (`version.py`). The project runs on newer builds, but the feature
  string is what the editor uses for compatibility warnings.

## GameManager (autoload)

Owns all state that survives a scene change and every scene transition:

- state: `lives` (3), `score`, `max_hp`/`current_hp` (5)
- signals: `score_changed`, `lives_changed`, `hp_changed`
- mutators: `reset_run`, `add_score`, `set_hp`, `heal`, `take_life`
  (`take_life` returns `false` when the run is over, otherwise refills HP)
- navigation: `goto_title`, `goto_splash`, `goto_level`, `goto_game_over`,
  `goto_victory` — all thin wrappers over `change_scene_to_file`

Scene flow: title → splash → `level_1` → victory (boss defeated) or
game_over (lives exhausted) → title.

## Animation

There is no `AnimationPlayer`, `AnimationTree` or state-machine resource
anywhere in the project. Every animated entity uses `AnimatedSprite2D` with a
`SpriteFrames` resource built at runtime by `scripts/anim_util.gd`:

```
AnimUtil.build([{name, path, count, w, h, fps, loop}, ...]) -> SpriteFrames
```

`build` slices a horizontal strip PNG into `AtlasTexture` frames. Each entity
declares its own `FRAMES_SPEC` constant and assigns the result in `_ready()`.

Consequences for anyone extending the game:

- Attack hitboxes are driven by timers in `_physics_process`, not by animation
  method tracks.
- Hit feedback is `sprite.modulate` strobing (`enemy_base.gd`) and
  `sprite.visible` strobing for player i-frames — no `ShaderMaterial`.
- No particle nodes are used (`CPUParticles2D`/`GPUParticles2D` are absent).

## Player (`scripts/player.gd`)

`CharacterBody2D`, in group `player`. Tunables are constants at the top of the
file: `SPEED 60`, `JUMP_VELOCITY -190`, `GRAVITY 700`, `CLIMB_SPEED 50`,
`INVINCIBLE_TIME 1.2`, `ATTACK_TIME 0.30`, `ATTACK_COOLDOWN 0.15`,
`ATTACK_DAMAGE 1`, knockback `(80, -120)`.

- Movement reads `Input.get_axis("move_left", "move_right")`.
- Attack: `AttackHitbox` (Area2D, group `player_attack`) is offset to
  `10 * facing` and enabled only while `attack_timer < ATTACK_TIME * 0.75`.
- Ladders: `ladders_overlapping` counter fed by `enter_ladder()`/`exit_ladder()`
  from the ladder areas built in `level_1.gd`; horizontal input or leaving the
  ladder cancels climbing.
- `take_damage(amount, from_position)` routes HP through `GameManager`; on 0 HP
  it calls `GameManager.take_life()` and then either respawns the level or goes
  to game over.
- `get_attack_damage()` is the contract enemies call to read damage.

## Enemies

`scripts/enemy_base.gd` (`CharacterBody2D`, group `enemy`) holds health,
contact damage, hit flash and the death handshake (award `score_value`, defer
`monitoring`/`monitorable` off on both areas, `queue_free`). Node contract:
optional `Sprite`, `Hurtbox`, `Hitbox`, `SfxHit` children.

| Enemy | Script | HP | Score | Behaviour |
| --- | --- | --- | --- | --- |
| Foot soldier | `foot_soldier.gd` | 2 | 100 | Patrols at 22, chases at 35 within 70 px; turns at walls and at ledges via a downward `intersect_ray` probe |
| Shuriken thrower | `shuriken_thrower.gd` | 3 | 150 | Paces ±14 px around spawn, fires `projectile.tscn` every 1.6 s within 110 px |
| Boss | `boss.gd` | 9 | 1000 | Hand-rolled `enum State { PAUSE, WINDUP, CHARGE, RECOVER }` in a `match`; charges at 70; emits `boss_hp_changed` / `boss_defeated` |

`projectile.gd` is an `Area2D` moving at 110 for 3 s, damaging `player` bodies
and despawning on `StaticBody2D`/`TileMap`.

`question_block.gd` is an `Area2D` consumed by the player body or by a
`player_attack` area: +50 score, +1 HP, then frees itself after its SFX.

## Level 1 (`scripts/level_1.gd`)

The level is generated in code — there is no hand-authored tilemap in the
`.tscn`. `_ready()` builds a `TileSet` (16 px tiles, one physics layer on
collision layer 1, 11 atlas tiles, full-tile collision polygons on brick /
concrete / roof), paints background and foreground layers, then spawns decor,
pickups, enemies, player, boss and HUD from the constant tables at the top of
the file (`PLATFORMS`, `LADDERS`, `SPIKES`, `PICKUPS`, `FOOT_SOLDIERS`,
`SHURIKEN_THROWERS`, `BOSS_POS`, `PLAYER_SPAWN`). World is 100x9 tiles.

Layout tweaks should be made in those tables, not in the scene file.

Note: the level uses the `TileMap` node, which is deprecated in this engine
version in favour of multiple `TileMapLayer` nodes (see
`doc/classes/TileMap.xml`). Migrating also means updating the `body is TileMap`
check in `projectile.gd`.

## HUD (`scripts/hud.gd`)

`CanvasLayer` that connects to the three `GameManager` signals, rebuilds the
heart row on every HP change, and shows the boss bar only after
`level_1.gd` calls `attach_boss(boss)`.

## Physics layers (`project.godot`)

1 world, 2 player, 3 player_hitbox, 4 enemy, 5 enemy_hitbox, 6 ladder,
7 pickup, 8 hazard.

Group names are used as much as layers: `player`, `player_attack`, `enemy`,
`boss`.

## Input actions

`move_left`, `move_right`, `jump`, `attack`, `climb_up`, `climb_down`, `start`
— each bound to keyboard and gamepad. See the controls table in
`steel_streets/README.md`.

## Assets

All sprites and audio are generated by `tools/gen_assets.py` (Pillow + `wave`);
regenerate with `python3 tools/gen_assets.py` rather than editing PNG/WAV files
by hand. Sprite sheets must stay horizontal strips of equal-sized frames for
`AnimUtil.build` to slice them.

## Running

```bash
# with a released editor
godot --path steel_streets

# with an engine built from this repo (blueprint build command)
scons tests=yes target=editor dev_build=yes -j$(nproc)
./bin/godot.linuxbsd.editor.dev.x86_64 --path steel_streets
```
