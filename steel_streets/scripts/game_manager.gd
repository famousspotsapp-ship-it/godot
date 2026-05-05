extends Node

## Autoload singleton that owns persistent run state and scene transitions.
##
## Tracks lives, score, and the player's current health between scenes, and
## centralises scene-change logic so the rest of the game just calls one of the
## four navigation helpers.

const STARTING_LIVES: int = 3
const STARTING_HP: int = 5

const TITLE_SCENE: String = "res://scenes/title_screen.tscn"
const SPLASH_SCENE: String = "res://scenes/splash_screen.tscn"
const LEVEL_SCENE: String = "res://scenes/level_1.tscn"
const GAME_OVER_SCENE: String = "res://scenes/game_over.tscn"
const VICTORY_SCENE: String = "res://scenes/victory.tscn"

var lives: int = STARTING_LIVES
var score: int = 0
var max_hp: int = STARTING_HP
var current_hp: int = STARTING_HP

signal score_changed(new_score: int)
signal lives_changed(new_lives: int)
signal hp_changed(current: int, maximum: int)


func reset_run() -> void:
	lives = STARTING_LIVES
	score = 0
	max_hp = STARTING_HP
	current_hp = STARTING_HP
	emit_signal("score_changed", score)
	emit_signal("lives_changed", lives)
	emit_signal("hp_changed", current_hp, max_hp)


func add_score(amount: int) -> void:
	score += amount
	emit_signal("score_changed", score)


func set_hp(value: int) -> void:
	current_hp = clampi(value, 0, max_hp)
	emit_signal("hp_changed", current_hp, max_hp)


func heal(amount: int) -> void:
	set_hp(current_hp + amount)


func take_life() -> bool:
	## Decrement lives. Returns true if the player still has lives remaining.
	lives -= 1
	emit_signal("lives_changed", lives)
	if lives <= 0:
		return false
	current_hp = max_hp
	emit_signal("hp_changed", current_hp, max_hp)
	return true


func goto_title() -> void:
	get_tree().change_scene_to_file(TITLE_SCENE)


func goto_splash() -> void:
	get_tree().change_scene_to_file(SPLASH_SCENE)


func goto_level() -> void:
	get_tree().change_scene_to_file(LEVEL_SCENE)


func goto_game_over() -> void:
	get_tree().change_scene_to_file(GAME_OVER_SCENE)


func goto_victory() -> void:
	get_tree().change_scene_to_file(VICTORY_SCENE)
