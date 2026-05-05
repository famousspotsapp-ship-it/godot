extends Control

## "LEVEL COMPLETE" screen. Press any key/button to return to the title.

@onready var score_label: Label = $Root/Score


func _ready() -> void:
	score_label.text = "FINAL SCORE %05d" % GameManager.score


func _input(event: InputEvent) -> void:
	if event is InputEventKey and event.pressed and not event.echo:
		_restart()
	elif event is InputEventJoypadButton and event.pressed:
		_restart()
	elif event is InputEventMouseButton and event.pressed:
		_restart()


func _restart() -> void:
	GameManager.goto_title()
