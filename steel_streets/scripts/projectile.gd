extends Area2D

## Enemy shuriken. Travels in a straight line, damages the player on contact,
## and despawns on world geometry or after its lifetime expires.

const AnimUtil := preload("res://scripts/anim_util.gd")
const FRAMES_SPEC := [
	{"name": "spin", "path": "res://assets/sprites/projectiles/shuriken.png", "count": 2, "w": 8, "h": 8, "fps": 16.0, "loop": true},
]

const SPEED: float = 110.0
const LIFETIME: float = 3.0
const DAMAGE: int = 1

@export var direction: int = 1
var life: float = LIFETIME

@onready var sprite: AnimatedSprite2D = $Sprite


func _ready() -> void:
	body_entered.connect(_on_body_entered)
	if sprite:
		sprite.sprite_frames = AnimUtil.build(FRAMES_SPEC)
		sprite.play("spin")
		sprite.flip_h = direction < 0


func _physics_process(delta: float) -> void:
	position.x += direction * SPEED * delta
	life -= delta
	if life <= 0.0:
		queue_free()


func _on_body_entered(body: Node) -> void:
	if body.is_in_group("player") and body.has_method("take_damage"):
		body.take_damage(DAMAGE, global_position)
		queue_free()
	elif body is TileMap or body is StaticBody2D:
		queue_free()
