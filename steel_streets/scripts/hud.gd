extends CanvasLayer

## Top-of-screen HUD: hearts, lives counter, score, and (optionally) the
## boss health bar shown when a boss is engaged.

const HEART_FULL := preload("res://assets/sprites/ui/heart.png")
const HEART_EMPTY := preload("res://assets/sprites/ui/heart_empty.png")

@onready var hearts_box: HBoxContainer = $Root/Top/Hearts
@onready var lives_label: Label = $Root/Top/Stats/Lives
@onready var score_label: Label = $Root/Top/Stats/Score
@onready var boss_root: Control = $Root/BossBar
@onready var boss_fill: ColorRect = $Root/BossBar/Bar/Fill
@onready var boss_label: Label = $Root/BossBar/Label


func _ready() -> void:
	GameManager.score_changed.connect(_on_score_changed)
	GameManager.lives_changed.connect(_on_lives_changed)
	GameManager.hp_changed.connect(_on_hp_changed)
	_on_score_changed(GameManager.score)
	_on_lives_changed(GameManager.lives)
	_on_hp_changed(GameManager.current_hp, GameManager.max_hp)
	boss_root.visible = false


func _on_score_changed(value: int) -> void:
	score_label.text = "%05d" % value


func _on_lives_changed(value: int) -> void:
	lives_label.text = "x%d" % maxi(value, 0)


func _on_hp_changed(current: int, maximum: int) -> void:
	for child in hearts_box.get_children():
		child.queue_free()
	for i in range(maximum):
		var t := TextureRect.new()
		t.texture = HEART_FULL if i < current else HEART_EMPTY
		t.stretch_mode = TextureRect.STRETCH_KEEP
		hearts_box.add_child(t)


func attach_boss(boss: Node) -> void:
	if not is_instance_valid(boss):
		return
	boss_root.visible = true
	boss.boss_hp_changed.connect(_on_boss_hp_changed)
	boss.boss_defeated.connect(_on_boss_defeated)


func _on_boss_hp_changed(current: int, maximum: int) -> void:
	var ratio: float = 0.0 if maximum <= 0 else clampf(float(current) / float(maximum), 0.0, 1.0)
	boss_fill.size.x = 64.0 * ratio
	boss_label.text = "BOSS"


func _on_boss_defeated() -> void:
	boss_root.visible = false
