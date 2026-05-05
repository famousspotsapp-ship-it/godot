extends Node2D

## Builds Level 1 procedurally: TileSet, TileMap (background + foreground),
## ladders, hazards, pickups, enemies, the boss, the player, and the camera.
## Doing it in code avoids hand-authoring a giant .tscn for the tilemap.

const TILE: int = 16
const WORLD_W: int = 100
const WORLD_H: int = 9
const GROUND_ROW: int = 8

# Atlas coordinates inside res://assets/sprites/tiles/tileset.png.
const A_BRICK := Vector2i(0, 0)
const A_CONCRETE := Vector2i(1, 0)
const A_ROOF := Vector2i(2, 0)
const A_BRICK_BG := Vector2i(3, 0)
const A_WINDOW := Vector2i(4, 0)
const A_DOOR := Vector2i(5, 0)
const A_SKY_DIAMOND := Vector2i(6, 0)
const A_SKY_ZIGZAG := Vector2i(7, 0)

const SOLID_TILES := [A_BRICK, A_CONCRETE, A_ROOF]

const PLAYER_SCENE := preload("res://scenes/player.tscn")
const FOOT_SOLDIER_SCENE := preload("res://scenes/foot_soldier.tscn")
const SHURIKEN_THROWER_SCENE := preload("res://scenes/shuriken_thrower.tscn")
const BOSS_SCENE := preload("res://scenes/boss.tscn")
const QUESTION_BLOCK_SCENE := preload("res://scenes/question_block.tscn")
const HUD_SCENE := preload("res://scenes/hud.tscn")
const TILESET_TEXTURE := preload("res://assets/sprites/tiles/tileset.png")
const LADDER_TEXTURE := preload("res://assets/sprites/items/ladder.png")
const SPIKE_TEXTURE := preload("res://assets/sprites/items/spike.png")

# Foreground rectangles painted with `atlas_coords`. (x0, y0, x1, y1) inclusive.
const PLATFORMS := [
	[0, 8, 99, 8, A_BRICK],          # ground
	[8, 5, 12, 5, A_CONCRETE],        # low floating platform
	[26, 4, 38, 4, A_ROOF],           # rooftop A
	[42, 4, 54, 4, A_ROOF],           # rooftop B (gap between A and B)
	[60, 5, 64, 5, A_CONCRETE],       # low floating platform
	[68, 4, 76, 4, A_ROOF],           # rooftop C
]

# Ladders defined as (column, top_row, bottom_row).
const LADDERS := [
	[27, 5, 7],
	[53, 5, 7],
	[69, 5, 7],
]

# Spikes (column, row). Sit on top of the ground (visually at row).
const SPIKES := [
	[18, 7],
	[36, 7],   # rooftop edge spikes
	[58, 7],
	[80, 7],
]

# Pickups (column, row).
const PICKUPS := [
	[5, 6],
	[10, 4],
	[33, 3],
	[48, 3],
	[62, 4],
	[72, 3],
]

# Enemies (column, row).
const FOOT_SOLDIERS := [
	[12, 7],
	[22, 7],
	[31, 3],
	[46, 3],
	[63, 7],
	[72, 7],
]
const SHURIKEN_THROWERS := [
	[40, 7],
	[78, 7],
]

const BOSS_POS := [92, 7]
const PLAYER_SPAWN := Vector2(32.0, 100.0)


@onready var entities: Node2D = $Entities
@onready var tilemap: TileMap = $TileMap
@onready var bgm: AudioStreamPlayer = $Bgm

var hud: CanvasLayer
var player: CharacterBody2D


func _ready() -> void:
	tilemap.tile_set = _build_tileset()
	# Layer 0 is implicit; add layer 1 for foreground/solid tiles.
	if tilemap.get_layers_count() < 2:
		tilemap.add_layer(-1)
	_paint_background()
	_paint_foreground()
	_spawn_decor()
	_spawn_pickups()
	_spawn_enemies()
	_spawn_player()
	_spawn_boss()
	_spawn_hud()
	if bgm.stream != null:
		bgm.play()


func _build_tileset() -> TileSet:
	var ts := TileSet.new()
	ts.tile_size = Vector2i(TILE, TILE)
	ts.add_physics_layer(0)
	ts.set_physics_layer_collision_layer(0, 1)  # world layer

	var src := TileSetAtlasSource.new()
	src.texture = TILESET_TEXTURE
	src.texture_region_size = Vector2i(TILE, TILE)
	# Link source to the tileset BEFORE creating tiles, so each new tile
	# inherits the physics layer count and we can set collision polygons.
	ts.add_source(src, 0)
	# 11 atlas tiles in a 1-row strip.
	for i in range(11):
		src.create_tile(Vector2i(i, 0))
	# Solid tiles get a full-tile collision polygon.
	var poly := PackedVector2Array([
		Vector2(-TILE * 0.5, -TILE * 0.5),
		Vector2(TILE * 0.5, -TILE * 0.5),
		Vector2(TILE * 0.5, TILE * 0.5),
		Vector2(-TILE * 0.5, TILE * 0.5),
	])
	for atlas in [A_BRICK, A_CONCRETE, A_ROOF]:
		var data: TileData = src.get_tile_data(atlas, 0)
		data.set_collision_polygons_count(0, 1)
		data.set_collision_polygon_points(0, 0, poly)
	return ts


func _paint_background() -> void:
	# Sky covers rows 0-3 across the world.
	for x in range(WORLD_W):
		for y in range(0, 4):
			tilemap.set_cell(0, Vector2i(x, y), 0, A_SKY_ZIGZAG)
		# Brick wall behind the city, rows 4-7 (where there is no foreground tile yet).
		for y in range(4, 8):
			tilemap.set_cell(0, Vector2i(x, y), 0, A_BRICK_BG)
	# Sprinkle windows on the brick wall.
	for x in range(2, WORLD_W, 5):
		tilemap.set_cell(0, Vector2i(x, 5), 0, A_WINDOW)
	# Doors at street level.
	for x in range(4, WORLD_W, 9):
		tilemap.set_cell(0, Vector2i(x, 7), 0, A_DOOR)
	# Decorative diamond clouds at the top.
	for x in range(6, WORLD_W, 14):
		tilemap.set_cell(0, Vector2i(x, 1), 0, A_SKY_DIAMOND)


func _paint_foreground() -> void:
	for entry in PLATFORMS:
		var x0: int = entry[0]
		var y0: int = entry[1]
		var x1: int = entry[2]
		var y1: int = entry[3]
		var atlas: Vector2i = entry[4]
		for x in range(x0, x1 + 1):
			for y in range(y0, y1 + 1):
				tilemap.set_cell(1, Vector2i(x, y), 0, atlas)


func _spawn_decor() -> void:
	# Ladders: rendered as Sprite2D children with an Area2D for climb detection.
	for entry in LADDERS:
		var col: int = entry[0]
		var top: int = entry[1]
		var bot: int = entry[2]
		_make_ladder(col, top, bot)
	for entry in SPIKES:
		_make_spike(entry[0], entry[1])


func _spawn_pickups() -> void:
	for entry in PICKUPS:
		var p := QUESTION_BLOCK_SCENE.instantiate()
		p.position = _tile_center(entry[0], entry[1])
		entities.add_child(p)


func _spawn_enemies() -> void:
	for entry in FOOT_SOLDIERS:
		var e := FOOT_SOLDIER_SCENE.instantiate()
		e.position = Vector2(entry[0] * TILE + TILE * 0.5, entry[1] * TILE + TILE)
		entities.add_child(e)
	for entry in SHURIKEN_THROWERS:
		var e := SHURIKEN_THROWER_SCENE.instantiate()
		e.position = Vector2(entry[0] * TILE + TILE * 0.5, entry[1] * TILE + TILE)
		entities.add_child(e)


func _spawn_player() -> void:
	player = PLAYER_SCENE.instantiate()
	player.position = PLAYER_SPAWN
	entities.add_child(player)
	# Camera2D follows the player and is constrained to the world bounds.
	var cam := Camera2D.new()
	cam.position_smoothing_enabled = true
	cam.position_smoothing_speed = 8.0
	cam.limit_left = 0
	cam.limit_top = 0
	cam.limit_right = WORLD_W * TILE
	cam.limit_bottom = WORLD_H * TILE
	cam.zoom = Vector2.ONE
	player.add_child(cam)


func _spawn_boss() -> void:
	var boss := BOSS_SCENE.instantiate()
	boss.position = Vector2(BOSS_POS[0] * TILE + TILE * 0.5, BOSS_POS[1] * TILE + TILE * 0.5)
	entities.add_child(boss)
	boss.boss_defeated.connect(_on_boss_defeated)
	# Hand the boss off to the HUD on the next frame, after HUD is ready.
	call_deferred("_attach_boss_to_hud", boss)


func _attach_boss_to_hud(boss: Node) -> void:
	if hud and hud.has_method("attach_boss"):
		hud.attach_boss(boss)


func _spawn_hud() -> void:
	hud = HUD_SCENE.instantiate()
	add_child(hud)


func _on_boss_defeated() -> void:
	await get_tree().create_timer(1.5).timeout
	if is_inside_tree():
		GameManager.goto_victory()


func _tile_center(col: int, row: int) -> Vector2:
	return Vector2(col * TILE + TILE * 0.5, row * TILE + TILE * 0.5)


func _make_ladder(col: int, top_row: int, bottom_row: int) -> void:
	var area := Area2D.new()
	area.name = "Ladder_%d" % col
	area.collision_layer = 32  # bit 5 (layer 6)
	area.collision_mask = 2    # bit 1 (layer 2 = player)
	area.add_to_group("ladder")
	# Visual sprites
	for y in range(top_row, bottom_row + 1):
		var s := Sprite2D.new()
		s.texture = LADDER_TEXTURE
		s.centered = true
		s.position = _tile_center(col, y) - _tile_center(col, top_row)
		area.add_child(s)
	# Collision shape spans the full vertical run.
	var shape := CollisionShape2D.new()
	var rect := RectangleShape2D.new()
	var height: float = (bottom_row - top_row + 1) * TILE
	rect.size = Vector2(TILE - 4, height)
	shape.shape = rect
	shape.position = Vector2(0, height * 0.5 - TILE * 0.5)
	area.add_child(shape)
	area.position = _tile_center(col, top_row)

	area.body_entered.connect(_on_ladder_body_entered)
	area.body_exited.connect(_on_ladder_body_exited)
	entities.add_child(area)


func _on_ladder_body_entered(body: Node) -> void:
	if body.has_method("enter_ladder"):
		body.enter_ladder()


func _on_ladder_body_exited(body: Node) -> void:
	if body.has_method("exit_ladder"):
		body.exit_ladder()


func _make_spike(col: int, row: int) -> void:
	var area := Area2D.new()
	area.name = "Spike_%d_%d" % [col, row]
	area.collision_layer = 128  # layer 8 (hazard)
	area.collision_mask = 2     # layer 2 (player)
	area.position = _tile_center(col, row)

	var s := Sprite2D.new()
	s.texture = SPIKE_TEXTURE
	s.centered = true
	area.add_child(s)

	var shape := CollisionShape2D.new()
	var rect := RectangleShape2D.new()
	rect.size = Vector2(TILE - 2, 6)
	shape.shape = rect
	shape.position = Vector2(0, 4)
	area.add_child(shape)

	area.body_entered.connect(func(body: Node) -> void:
		if body.is_in_group("player") and body.has_method("take_damage"):
			body.take_damage(1, area.global_position)
	)
	entities.add_child(area)
