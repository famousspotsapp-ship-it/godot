extends "res://scripts/enemy_base.gd"

## End-of-level boss. Cycles through "wait → wind-up → charge → recover" and
## takes 8-10 hits to defeat.

const FRAMES_SPEC := [
	{"name": "idle", "path": "res://assets/sprites/enemies/boss.png", "count": 2, "w": 24, "h": 32, "fps": 2.5, "loop": true},
]

signal boss_hp_changed(current: int, maximum: int)
signal boss_defeated

const GRAVITY: float = 700.0
const CHARGE_SPEED: float = 70.0
const PAUSE_TIME: float = 1.0
const WINDUP_TIME: float = 0.5
const CHARGE_TIME: float = 1.0
const RECOVER_TIME: float = 0.7

enum State { PAUSE, WINDUP, CHARGE, RECOVER }

var state: int = State.PAUSE
var state_timer: float = PAUSE_TIME
var direction: int = -1


func _ready() -> void:
	max_hp = 9
	contact_damage = 1
	score_value = 1000
	super()
	add_to_group("boss")
	hp = max_hp
	if sprite:
		sprite.sprite_frames = AnimUtil.build(FRAMES_SPEC)
		sprite.play("idle")
	emit_signal("boss_hp_changed", hp, max_hp)


func _physics_process(delta: float) -> void:
	if dead:
		return

	velocity.y += GRAVITY * delta

	state_timer -= delta
	var player: Node = get_tree().get_first_node_in_group("player")
	if player and is_instance_valid(player):
		direction = -1 if player.global_position.x < global_position.x else 1

	match state:
		State.PAUSE:
			velocity.x = 0.0
			if sprite:
				sprite.play("idle")
				sprite.flip_h = direction > 0
			if state_timer <= 0.0:
				state = State.WINDUP
				state_timer = WINDUP_TIME
		State.WINDUP:
			velocity.x = -direction * 10.0
			if sprite:
				sprite.modulate = Color(1.4, 1.4, 1.4, 1)
			if state_timer <= 0.0:
				state = State.CHARGE
				state_timer = CHARGE_TIME
				if sprite:
					sprite.modulate = Color(1, 1, 1, 1)
		State.CHARGE:
			velocity.x = direction * CHARGE_SPEED
			if is_on_wall():
				state = State.RECOVER
				state_timer = RECOVER_TIME
			elif state_timer <= 0.0:
				state = State.RECOVER
				state_timer = RECOVER_TIME
		State.RECOVER:
			velocity.x = 0.0
			if state_timer <= 0.0:
				state = State.PAUSE
				state_timer = PAUSE_TIME

	move_and_slide()


func take_damage(amount: int) -> void:
	if dead:
		return
	hp -= amount
	hit_flash_timer = 0.18
	emit_signal("boss_hp_changed", hp, max_hp)
	if sfx_hit and sfx_hit.stream != null:
		sfx_hit.play()
	if hp <= 0:
		_die()


func _die() -> void:
	if dead:
		return
	dead = true
	GameManager.add_score(score_value)
	emit_signal("boss_defeated")
	if hurtbox:
		hurtbox.set_deferred("monitoring", false)
		hurtbox.set_deferred("monitorable", false)
	if hitbox:
		hitbox.set_deferred("monitoring", false)
		hitbox.set_deferred("monitorable", false)
	queue_free()
