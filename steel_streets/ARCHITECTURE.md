# Steel Streets — architecture reference

Written against the code in `steel_streets/` as of engine version 4.7-beta
(`version.py`). It documents how the game is *actually* wired, because the
generated repository wiki describes several mechanics (AnimationPlayer-driven
hitboxes, shader-based hit flash, an AnimationTree boss) that do not exist here.

## Engine / project versions

- The engine in this repository is **4.7-beta**; `project.godot` still declares
  `config/features=PackedStringArray("4.3", "GL Compatibility")`, so the project
  opens in any 4.3+ editor and does not use post-4.3 APIs.
- Renderer: `gl_compatibility` (desktop and mobile), default texture filter
  `nearest` (`textures/canvas_textures/default_texture_filter=0`).
- Viewport is 160x144 with `stretch/mode="viewport"` and `aspect="keep"`;
  the window opens at 640x576.

## Runtime structure

| Piece | File | Notes |
| --- | --- | --- |
| Autoload singleton | `scripts/game_manager.gd` | Run state + scene routing |
| Level | `scripts/level_1.gd` | Builds the whole level in code |
| Player | `scripts/player.gd` | `CharacterBody2D`, timer-driven states |
| Enemy base | `scripts/enemy_base.gd` | HP, contact damage, death handshake |
| Enemies | `foot_soldier.gd`, `shuriken_thrower.gd`, `boss.gd` | Extend the base script by path |
| Projectile | `scripts/projectile.gd` | `Area2D`, straight line, 3 s lifetime |
| Pickup | `scripts/question_block.gd` | `Area2D`, score + heal, one shot |
| HUD | `scripts/hud.gd` | `CanvasLayer`, driven by GameManager signals |
| Animation helper | `scripts/anim_util.gd` | Builds `SpriteFrames` from sprite strips |

### GameManager (autoload `GameManager`)

State: `lives` (3), `score`, `max_hp`/`current_hp` (5).
Signals: `score_changed`, `lives_changed`, `hp_changed`.
API: `reset_run()`, `add_score(amount)`, `set_hp(value)`, `heal(amount)`,
`take_life() -> bool`, and the navigation helpers `goto_title()`,
`goto_splash()`, `goto_level()`, `goto_game_over()`, `goto_victory()`.
Scene changes go through `get_tree().change_scene_to_file()` inside those
helpers — callers never build scene paths themselves.

### Level 1 is generated, not authored

`scenes/level_1.tscn` only holds a `TileMap`, an `Entities` node and the BGM
player: `level_1.gd` procedurally builds the `TileSet` from
`assets/sprites/tiles/tileset.png` and paints the tiles, then spawns
platforms, ladders (`Area2D`, group `ladder`), hazards, question blocks,
enemies, the boss, the player, the HUD and the camera. Level layout lives in
the `PLATFORMS` / spawn constants at the top of that script — edit those, not a
scene file. The world is 100x9 tiles of 16 px, ground row 8.

Level completion: `level_1.gd` connects the boss's `boss_defeated` signal and
calls `GameManager.goto_victory()`.

### Animation

There is no `AnimationPlayer` or `AnimationTree` anywhere in the project.
Every animated entity is an `AnimatedSprite2D` whose `SpriteFrames` are built at
runtime by `AnimUtil.build(FRAMES_SPEC)` (`scripts/anim_util.gd`), which slices
horizontal sprite strips into `AtlasTexture` frames using the per-animation
`count`/`w`/`h`/`fps`/`loop` values declared in each script's `FRAMES_SPEC`.

### Combat

- Player attack is **timer driven**: `ATTACK_TIME` 0.30 s, `ATTACK_COOLDOWN`
  0.15 s. `_update_attack_state()` toggles `monitoring` on the `AttackHitbox`
  `Area2D` (group `player_attack`) and its `CollisionShape2D`.
- Damage to enemies flows `player_attack` area -> `EnemyBase.hurtbox`
  (`area_entered`) -> `take_damage()`, asking the attacker for
  `get_attack_damage()`.
- Contact damage flows `EnemyBase.hitbox` (`body_entered`) -> player
  `take_damage(amount, from_position)`.
- Hit flash is `sprite.modulate` toggled between `Color(2, 2, 2)` and white for
  0.18 s in `EnemyBase._process()` — no shader, no material.
- Death: `EnemyBase._die()` adds score, deferred-disables hurtbox/hitbox, and
  `queue_free()`s the node. No death animation or method track.
- Player invincibility is 1.2 s (`INVINCIBLE_TIME`) with a flashing sprite;
  knockback is applied directly to `velocity`.
- Boss (`boss.gd`): 9 HP, 1000 points, a GDScript `enum State { PAUSE, WINDUP,
  CHARGE, RECOVER }` timer state machine with a single charge attack, emitting
  `boss_hp_changed` and `boss_defeated`.
- Shuriken thrower: 3 HP, paces +/-14 px around its spawn point, fires every
  1.6 s within 110 px of the player.
- Question blocks are consumed by walking into them (`body_entered`) *or* by
  hitting them with the attack hitbox (`area_entered`) — not by hitting them
  from below — and grant score plus a heal through `GameManager`.

### Physics layers (`project.godot` -> `[layer_names]`)

1. `world` 2. `player` 3. `player_hitbox` 4. `enemy` 5. `enemy_hitbox`
6. `ladder` 7. `pickup` 8. `hazard`

Cross-entity lookups use groups rather than layers: `player`, `player_attack`,
`enemy`, `boss`, `ladder`.

Note the level still uses the `TileMap` node, which is deprecated in this engine
in favor of one `TileMapLayer` per layer; migrating means reworking the
`tilemap.set_cell(layer, ...)` calls in `level_1.gd`.

## Working on the game

```bash
# Run with any Godot 4.3+ binary, or a build from this repo:
godot --path steel_streets
./bin/godot.linuxbsd.editor.dev.x86_64 --path steel_streets

# Regenerate every sprite and sound effect (requires Pillow):
python3 steel_streets/tools/gen_assets.py
```

Assets under `assets/` are all generated by `tools/gen_assets.py`; change the
generator, not the PNG/WAV files. Nothing in `steel_streets/` is covered by the
engine's CI workflows or by `scons` builds.
