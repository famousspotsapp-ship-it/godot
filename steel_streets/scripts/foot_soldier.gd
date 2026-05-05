extends "res://scripts/enemy_base.gd"

## Patrolling melee grunt. Walks back and forth along its platform, and
## charges toward the player when the player is in chase range.

const FRAMES_SPEC := [
	{"name": "walk", "path": "res://assets/sprites/enemies/foot_soldier.png", "count": 2, "w": 16, "h": 16, "fps": 4.0, "loop": true},
]

const PATROL_SPEED: float = 22.0
const CHASE_SPEED: float = 35.0
const GRAVITY: float = 700.0
const CHASE_RANGE: float = 70.0
const TURN_AROUND_DELAY: float = 0.4

var direction: int = -1
var turn_timer: float = 0.0


func _ready() -> void:
	max_hp = 2
	contact_damage = 1
	score_value = 100
	super()
	if sprite:
		sprite.sprite_frames = AnimUtil.build(FRAMES_SPEC)
		sprite.play("walk")


func _physics_process(delta: float) -> void:
	if dead:
		return

	velocity.y += GRAVITY * delta

	var player: Node = get_tree().get_first_node_in_group("player")
	var speed: float = PATROL_SPEED
	if player and is_instance_valid(player):
		var dx: float = player.global_position.x - global_position.x
		if absf(dx) < CHASE_RANGE and absf(player.global_position.y - global_position.y) < 24.0:
			direction = -1 if dx < 0.0 else 1
			speed = CHASE_SPEED

	if turn_timer > 0.0:
		turn_timer -= delta
		velocity.x = 0.0
	else:
		velocity.x = direction * speed
		# Turn at walls or ledges.
		if is_on_wall():
			direction *= -1
			turn_timer = TURN_AROUND_DELAY
		elif is_on_floor() and not _has_floor_ahead():
			direction *= -1
			turn_timer = TURN_AROUND_DELAY

	if sprite:
		sprite.flip_h = direction > 0

	move_and_slide()


func _has_floor_ahead() -> bool:
	var space := get_world_2d().direct_space_state
	var origin := global_position + Vector2(direction * 8.0, 0.0)
	var to := origin + Vector2(0.0, 16.0)
	var query := PhysicsRayQueryParameters2D.create(origin, to, 1)
	query.exclude = [self]
	var result := space.intersect_ray(query)
	return not result.is_empty()
