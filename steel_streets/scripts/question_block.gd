extends Area2D

## Pickup block. Awards score and (optionally) heals the player when the
## player either walks into it or hits it with an attack. Disappears on use.

@export var score_reward: int = 50
@export var heal_amount: int = 1

@onready var sfx: AudioStreamPlayer = $SfxPickup if has_node("SfxPickup") else null
@onready var sprite: Sprite2D = $Sprite if has_node("Sprite") else null

var consumed: bool = false


func _ready() -> void:
	body_entered.connect(_on_body_entered)
	area_entered.connect(_on_area_entered)


func _on_body_entered(body: Node) -> void:
	if body.is_in_group("player"):
		_consume(body)


func _on_area_entered(area: Area2D) -> void:
	if area.is_in_group("player_attack"):
		var p: Node = get_tree().get_first_node_in_group("player")
		_consume(p)


func _consume(player: Node) -> void:
	if consumed:
		return
	consumed = true
	GameManager.add_score(score_reward)
	if heal_amount > 0 and player and player.has_method("get_attack_damage"):
		GameManager.heal(heal_amount)
	if sfx and sfx.stream != null:
		sfx.play()
		# Wait for the sfx to finish before freeing.
		if sprite:
			sprite.visible = false
		set_deferred("monitoring", false)
		set_deferred("monitorable", false)
		await sfx.finished
		queue_free()
	else:
		queue_free()
