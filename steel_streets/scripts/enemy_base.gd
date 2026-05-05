extends CharacterBody2D

## Base class for all enemies. Owns health, contact damage, and the death
## handshake (drop score, hide hitboxes, queue_free).

const AnimUtil := preload("res://scripts/anim_util.gd")

@export var max_hp: int = 2
@export var contact_damage: int = 1
@export var score_value: int = 100

@onready var sprite: AnimatedSprite2D = $Sprite if has_node("Sprite") else null
@onready var hurtbox: Area2D = $Hurtbox if has_node("Hurtbox") else null
@onready var hitbox: Area2D = $Hitbox if has_node("Hitbox") else null
@onready var sfx_hit: AudioStreamPlayer = $SfxHit if has_node("SfxHit") else null

var hp: int = 0
var hit_flash_timer: float = 0.0
var dead: bool = false


func _ready() -> void:
	add_to_group("enemy")
	hp = max_hp
	if hurtbox:
		hurtbox.area_entered.connect(_on_hurtbox_area_entered)
	if hitbox:
		hitbox.body_entered.connect(_on_hitbox_body_entered)


func _process(delta: float) -> void:
	if hit_flash_timer > 0.0:
		hit_flash_timer -= delta
		if sprite:
			sprite.modulate = Color(2.0, 2.0, 2.0, 1.0) if int(hit_flash_timer * 20) % 2 == 0 else Color(1, 1, 1, 1)
	elif sprite and sprite.modulate != Color(1, 1, 1, 1):
		sprite.modulate = Color(1, 1, 1, 1)


func _on_hurtbox_area_entered(area: Area2D) -> void:
	if dead:
		return
	if area.is_in_group("player_attack"):
		var dmg: int = 1
		var owner_node: Node = area.get_parent()
		if owner_node and owner_node.has_method("get_attack_damage"):
			dmg = owner_node.get_attack_damage()
		take_damage(dmg)


func _on_hitbox_body_entered(body: Node) -> void:
	if dead:
		return
	if body.is_in_group("player") and body.has_method("take_damage"):
		body.take_damage(contact_damage, global_position)


func take_damage(amount: int) -> void:
	if dead:
		return
	hp -= amount
	hit_flash_timer = 0.18
	if sfx_hit and sfx_hit.stream != null:
		sfx_hit.play()
	if hp <= 0:
		_die()


func _die() -> void:
	dead = true
	GameManager.add_score(score_value)
	if hurtbox:
		hurtbox.set_deferred("monitoring", false)
		hurtbox.set_deferred("monitorable", false)
	if hitbox:
		hitbox.set_deferred("monitoring", false)
		hitbox.set_deferred("monitorable", false)
	queue_free()
