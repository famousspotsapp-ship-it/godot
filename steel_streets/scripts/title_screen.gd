extends Control

## Title screen. Blinks "PRESS START" and waits for any common input to
## advance to the splash screen.

@onready var press_label: TextureRect = $Root/Press
var blink_t: float = 0.0


func _ready() -> void:
	GameManager.reset_run()


func _process(delta: float) -> void:
	blink_t += delta
	press_label.visible = fmod(blink_t, 1.0) < 0.6


func _input(event: InputEvent) -> void:
	if event is InputEventKey and event.pressed and not event.echo:
		_advance()
	elif event is InputEventJoypadButton and event.pressed:
		_advance()
	elif event is InputEventMouseButton and event.pressed:
		_advance()


func _advance() -> void:
	GameManager.goto_splash()
