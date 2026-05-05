extends CharacterBody2D

## Player character controller. Handles walking, jumping, melee attack, ladder
## climbing, and damage with brief invincibility flashing.

const AnimUtil := preload("res://scripts/anim_util.gd")
const FRAMES_SPEC := [
	{"name": "idle",   "path": "res://assets/sprites/player/idle.png",   "count": 2, "w": 16, "h": 24, "fps": 3.0,  "loop": true},
	{"name": "walk",   "path": "res://assets/sprites/player/walk.png",   "count": 4, "w": 16, "h": 24, "fps": 8.0,  "loop": true},
	{"name": "jump",   "path": "res://assets/sprites/player/jump.png",   "count": 2, "w": 16, "h": 24, "fps": 6.0,  "loop": true},
	{"name": "attack", "path": "res://assets/sprites/player/attack.png", "count": 3, "w": 16, "h": 24, "fps": 12.0, "loop": false},
	{"name": "climb",  "path": "res://assets/sprites/player/climb.png",  "count": 2, "w": 16, "h": 24, "fps": 6.0,  "loop": true},
	{"name": "hurt",   "path": "res://assets/sprites/player/hurt.png",   "count": 1, "w": 16, "h": 24, "fps": 1.0,  "loop": false},
]

const SPEED: float = 60.0
const JUMP_VELOCITY: float = -190.0
const GRAVITY: float = 700.0
const CLIMB_SPEED: float = 50.0
const KNOCKBACK_X: float = 80.0
const KNOCKBACK_Y: float = -120.0
const INVINCIBLE_TIME: float = 1.2
const ATTACK_TIME: float = 0.30
const ATTACK_COOLDOWN: float = 0.15
const ATTACK_DAMAGE: int = 1

@onready var sprite: AnimatedSprite2D = $Sprite
@onready var attack_hitbox: Area2D = $AttackHitbox
@onready var attack_shape: CollisionShape2D = $AttackHitbox/Shape
@onready var sfx_jump: AudioStreamPlayer = $SfxJump
@onready var sfx_attack: AudioStreamPlayer = $SfxAttack
@onready var sfx_hurt: AudioStreamPlayer = $SfxHurt

var facing: int = 1
var attack_timer: float = 0.0
var cooldown_timer: float = 0.0
var invincible_timer: float = 0.0
var ladders_overlapping: int = 0
var climbing: bool = false
var dead: bool = false


func _ready() -> void:
	add_to_group("player")
	sprite.sprite_frames = AnimUtil.build(FRAMES_SPEC)
	sprite.play("idle")
	attack_hitbox.monitoring = false
	attack_shape.disabled = true
	attack_hitbox.add_to_group("player_attack")
	GameManager.set_hp(GameManager.max_hp)


func _physics_process(delta: float) -> void:
	if dead:
		velocity.y += GRAVITY * delta
		move_and_slide()
		return

	_tick_timers(delta)

	if climbing:
		_handle_climbing(delta)
	else:
		_handle_grounded(delta)

	_update_attack_state()
	_update_animation()
	_update_invincibility_flash()
	move_and_slide()


func _tick_timers(delta: float) -> void:
	if attack_timer > 0.0:
		attack_timer -= delta
	if cooldown_timer > 0.0:
		cooldown_timer -= delta
	if invincible_timer > 0.0:
		invincible_timer -= delta


func _handle_grounded(delta: float) -> void:
	velocity.y += GRAVITY * delta

	var dir: float = Input.get_axis("move_left", "move_right")
	velocity.x = dir * SPEED
	if dir > 0.01:
		facing = 1
	elif dir < -0.01:
		facing = -1

	if Input.is_action_just_pressed("jump") and is_on_floor():
		velocity.y = JUMP_VELOCITY
		if sfx_jump.stream != null:
			sfx_jump.play()

	if Input.is_action_just_pressed("attack") and attack_timer <= 0.0 and cooldown_timer <= 0.0:
		_start_attack()

	if ladders_overlapping > 0:
		var want_up := Input.is_action_pressed("climb_up")
		var want_down := Input.is_action_pressed("climb_down") and not is_on_floor()
		if want_up or want_down:
			climbing = true
			velocity = Vector2.ZERO


func _handle_climbing(delta: float) -> void:
	if ladders_overlapping <= 0:
		climbing = false
		return

	var horizontal: float = Input.get_axis("move_left", "move_right")
	if absf(horizontal) > 0.1:
		climbing = false
		return

	var vy: float = 0.0
	if Input.is_action_pressed("climb_up"):
		vy -= CLIMB_SPEED
	if Input.is_action_pressed("climb_down"):
		vy += CLIMB_SPEED
	velocity.x = 0.0
	velocity.y = vy

	if Input.is_action_just_pressed("jump"):
		climbing = false
		velocity.y = JUMP_VELOCITY * 0.7

	if Input.is_action_just_pressed("attack") and attack_timer <= 0.0 and cooldown_timer <= 0.0:
		_start_attack()


func _start_attack() -> void:
	attack_timer = ATTACK_TIME
	cooldown_timer = ATTACK_TIME + ATTACK_COOLDOWN
	if sfx_attack.stream != null:
		sfx_attack.play()


func _update_attack_state() -> void:
	# Hitbox active only during the swing window (middle of the animation).
	var swinging := attack_timer > 0.0 and attack_timer < ATTACK_TIME * 0.75
	attack_hitbox.position.x = 10.0 * facing
	attack_shape.disabled = not swinging
	attack_hitbox.monitoring = swinging


func _update_animation() -> void:
	sprite.flip_h = facing < 0
	if attack_timer > 0.0:
		sprite.play("attack")
		return
	if climbing:
		sprite.play("climb")
		if absf(velocity.y) < 1.0:
			sprite.pause()
		return
	if not is_on_floor():
		sprite.play("jump")
		return
	if absf(velocity.x) > 1.0:
		sprite.play("walk")
	else:
		sprite.play("idle")


func _update_invincibility_flash() -> void:
	if invincible_timer > 0.0:
		# Strobe at 12 Hz.
		sprite.visible = int(invincible_timer * 12.0) % 2 == 0
	else:
		sprite.visible = true


func enter_ladder() -> void:
	ladders_overlapping += 1


func exit_ladder() -> void:
	ladders_overlapping = maxi(0, ladders_overlapping - 1)
	if ladders_overlapping == 0:
		climbing = false


func take_damage(amount: int, from_position: Vector2) -> void:
	if invincible_timer > 0.0 or dead:
		return
	var new_hp: int = GameManager.current_hp - amount
	GameManager.set_hp(new_hp)
	invincible_timer = INVINCIBLE_TIME
	if sfx_hurt.stream != null:
		sfx_hurt.play()
	# Knockback away from source.
	var dir: float = -1.0 if from_position.x > global_position.x else 1.0
	velocity.x = KNOCKBACK_X * dir
	velocity.y = KNOCKBACK_Y
	climbing = false
	if new_hp <= 0:
		_die()


func _die() -> void:
	if dead:
		return
	dead = true
	sprite.play("hurt")
	attack_hitbox.monitoring = false
	attack_shape.disabled = true
	# Lose a life and either respawn or game-over.
	var still_alive: bool = GameManager.take_life()
	if still_alive:
		await get_tree().create_timer(1.0).timeout
		GameManager.goto_level()
	else:
		await get_tree().create_timer(1.2).timeout
		GameManager.goto_game_over()


func get_attack_damage() -> int:
	return ATTACK_DAMAGE
