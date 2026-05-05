extends "res://scripts/enemy_base.gd"

## Stationary ranged attacker that paces a small range and lobs shurikens
## horizontally toward the player at a fixed cadence.

const PROJECTILE_SCENE: PackedScene = preload("res://scenes/projectile.tscn")
const FRAMES_SPEC := [
	{"name": "idle", "path": "res://assets/sprites/enemies/shuriken_thrower.png", "count": 2, "w": 16, "h": 16, "fps": 2.5, "loop": true},
]
const PACE_SPEED: float = 12.0
const PACE_RANGE: float = 14.0
const GRAVITY: float = 700.0
const FIRE_INTERVAL: float = 1.6
const SIGHT_RANGE: float = 110.0

var origin_x: float = 0.0
var fire_timer: float = 0.0
var direction: int = -1


func _ready() -> void:
	max_hp = 3
	contact_damage = 1
	score_value = 150
	super()
	origin_x = global_position.x
	fire_timer = FIRE_INTERVAL * 0.5
	if sprite:
		sprite.sprite_frames = AnimUtil.build(FRAMES_SPEC)
		sprite.play("idle")


func _physics_process(delta: float) -> void:
	if dead:
		return
	velocity.y += GRAVITY * delta

	# Pace within a small range around the spawn point.
	if global_position.x < origin_x - PACE_RANGE:
		direction = 1
	elif global_position.x > origin_x + PACE_RANGE:
		direction = -1
	velocity.x = direction * PACE_SPEED

	# Face and shoot toward the player when in sight.
	var player: Node = get_tree().get_first_node_in_group("player")
	if player and is_instance_valid(player):
		var dx: float = player.global_position.x - global_position.x
		if absf(dx) < SIGHT_RANGE and absf(player.global_position.y - global_position.y) < 32.0:
			var face: int = -1 if dx < 0.0 else 1
			if sprite:
				sprite.flip_h = face > 0
			fire_timer -= delta
			if fire_timer <= 0.0:
				_fire(face)
				fire_timer = FIRE_INTERVAL

	move_and_slide()


func _fire(face_dir: int) -> void:
	var p := PROJECTILE_SCENE.instantiate()
	p.global_position = global_position + Vector2(face_dir * 8.0, -2.0)
	p.set("direction", face_dir)
	get_parent().add_child(p)
