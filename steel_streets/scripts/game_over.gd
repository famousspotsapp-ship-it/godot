extends Control

## "GAME OVER" screen. Press any key/button to restart.


func _input(event: InputEvent) -> void:
	if event is InputEventKey and event.pressed and not event.echo:
		_restart()
	elif event is InputEventJoypadButton and event.pressed:
		_restart()
	elif event is InputEventMouseButton and event.pressed:
		_restart()


func _restart() -> void:
	GameManager.goto_title()
