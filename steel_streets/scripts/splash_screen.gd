extends Control

## Character splash. Auto-advances after a short timer or on any input.

const HOLD_TIME: float = 2.5


func _ready() -> void:
	await get_tree().create_timer(HOLD_TIME).timeout
	if is_inside_tree():
		GameManager.goto_level()


func _input(event: InputEvent) -> void:
	if event is InputEventKey and event.pressed and not event.echo:
		_skip()
	elif event is InputEventJoypadButton and event.pressed:
		_skip()
	elif event is InputEventMouseButton and event.pressed:
		_skip()


func _skip() -> void:
	if is_inside_tree():
		GameManager.goto_level()
