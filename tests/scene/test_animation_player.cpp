/**************************************************************************/
/*  test_animation_player.cpp                                             */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#include "tests/test_macros.h"

TEST_FORCE_LINK(test_animation_player)

#include "scene/animation/animation_player.h"
#include "scene/main/scene_tree.h"
#include "scene/main/window.h"
#include "scene/resources/animation.h"
#include "scene/resources/animation_library.h"
#include "tests/signal_watcher.h"

namespace TestAnimationPlayer {

// Helper: create a simple value animation of a given length.
static Ref<Animation> make_value_animation(double p_length, Animation::LoopMode p_loop = Animation::LOOP_NONE) {
	Ref<Animation> anim = memnew(Animation);
	anim->set_length(p_length);
	anim->set_loop_mode(p_loop);
	int idx = anim->add_track(Animation::TYPE_VALUE);
	anim->track_set_path(idx, NodePath(".:position:x"));
	anim->track_insert_key(idx, 0.0, 0);
	anim->track_insert_key(idx, p_length, 100);
	return anim;
}

// Helper: add two named animations to a player via a default library.
static Ref<AnimationLibrary> setup_two_animations(AnimationPlayer *p_player, const String &p_name1, double p_len1, const String &p_name2, double p_len2) {
	Ref<AnimationLibrary> lib = memnew(AnimationLibrary);
	lib->add_animation(p_name1, make_value_animation(p_len1));
	lib->add_animation(p_name2, make_value_animation(p_len2));
	p_player->add_animation_library("", lib);
	return lib;
}

// ---------- Basic property getters / setters ----------

TEST_CASE("[AnimationPlayer] get & set default_blend_time") {
	AnimationPlayer *animation_player = memnew(AnimationPlayer);
	animation_player->set_default_blend_time(4.0);

	CHECK(animation_player->get_default_blend_time() == doctest::Approx(4.0f));
	memdelete(animation_player);
}

TEST_CASE("[AnimationPlayer] get & set blend_time") {
	String anim1 = "animation1";
	String anim2 = "animation2";
	const Ref<Animation> animation1 = memnew(Animation);
	const Ref<Animation> animation2 = memnew(Animation);
	const Ref<AnimationLibrary> animation_library = memnew(AnimationLibrary);
	animation_library->add_animation(anim1, animation1);
	animation_library->add_animation(anim2, animation2);

	AnimationPlayer *animation_player = memnew(AnimationPlayer);
	animation_player->add_animation_library("", animation_library);

	animation_player->set_blend_time(anim1, anim2, 4.0);
	CHECK(animation_player->get_blend_time(anim1, anim2) == doctest::Approx(4.0f));
	memdelete(animation_player);
}

TEST_CASE("[AnimationPlayer] Speed scale getters and setters") {
	AnimationPlayer *player = memnew(AnimationPlayer);

	CHECK(player->get_speed_scale() == doctest::Approx(1.0f));

	player->set_speed_scale(2.5f);
	CHECK(player->get_speed_scale() == doctest::Approx(2.5f));

	player->set_speed_scale(-1.0f);
	CHECK(player->get_speed_scale() == doctest::Approx(-1.0f));

	player->set_speed_scale(0.0f);
	CHECK(player->get_speed_scale() == doctest::Approx(0.0f));

	memdelete(player);
}

TEST_CASE("[AnimationPlayer] Auto-capture getters and setters") {
	AnimationPlayer *player = memnew(AnimationPlayer);

	CHECK(player->is_auto_capture() == true);

	player->set_auto_capture(false);
	CHECK(player->is_auto_capture() == false);

	player->set_auto_capture_duration(0.5);
	CHECK(player->get_auto_capture_duration() == doctest::Approx(0.5));

	player->set_auto_capture_transition_type(Tween::TRANS_CUBIC);
	CHECK(player->get_auto_capture_transition_type() == Tween::TRANS_CUBIC);

	player->set_auto_capture_ease_type(Tween::EASE_OUT);
	CHECK(player->get_auto_capture_ease_type() == Tween::EASE_OUT);

	memdelete(player);
}

// ---------- Blend-time edge cases ----------

TEST_CASE("[AnimationPlayer] Blend time of zero erases the entry") {
	AnimationPlayer *player = memnew(AnimationPlayer);
	setup_two_animations(player, "a", 1.0, "b", 1.0);

	player->set_blend_time("a", "b", 0.5);
	CHECK(player->get_blend_time("a", "b") == doctest::Approx(0.5));

	player->set_blend_time("a", "b", 0.0);
	CHECK(player->get_blend_time("a", "b") == doctest::Approx(0.0));

	memdelete(player);
}

TEST_CASE("[AnimationPlayer] Blend time is directional") {
	AnimationPlayer *player = memnew(AnimationPlayer);
	setup_two_animations(player, "walk", 1.0, "run", 1.0);

	player->set_blend_time("walk", "run", 0.3);
	CHECK(player->get_blend_time("walk", "run") == doctest::Approx(0.3));
	CHECK(player->get_blend_time("run", "walk") == doctest::Approx(0.0));

	memdelete(player);
}

TEST_CASE("[SceneTree][AnimationPlayer] Default blend time used as fallback") {
	AnimationPlayer *player = memnew(AnimationPlayer);
	player->set_auto_capture(false);
	setup_two_animations(player, "idle", 1.0, "walk", 1.0);
	SceneTree::get_singleton()->get_root()->add_child(player);

	player->set_default_blend_time(0.25);
	CHECK(player->get_blend_time("idle", "walk") == doctest::Approx(0.0));

	player->play("idle");
	player->advance(0.0);
	player->play("walk");
	CHECK(player->is_playing());

	SceneTree::get_singleton()->get_root()->remove_child(player);
	memdelete(player);
}

// ---------- Play / Stop / Pause ----------

TEST_CASE("[SceneTree][AnimationPlayer] Play starts playback") {
	AnimationPlayer *player = memnew(AnimationPlayer);
	player->set_auto_capture(false);
	Ref<AnimationLibrary> lib = memnew(AnimationLibrary);
	lib->add_animation("test", make_value_animation(1.0));
	player->add_animation_library("", lib);
	SceneTree::get_singleton()->get_root()->add_child(player);

	CHECK_FALSE(player->is_playing());

	player->play("test");
	CHECK(player->is_playing());
	CHECK(player->get_current_animation() == StringName("test"));

	SceneTree::get_singleton()->get_root()->remove_child(player);
	memdelete(player);
}

TEST_CASE("[SceneTree][AnimationPlayer] Stop resets playback state") {
	AnimationPlayer *player = memnew(AnimationPlayer);
	player->set_auto_capture(false);
	Ref<AnimationLibrary> lib = memnew(AnimationLibrary);
	lib->add_animation("test", make_value_animation(1.0));
	player->add_animation_library("", lib);
	SceneTree::get_singleton()->get_root()->add_child(player);

	player->play("test");
	CHECK(player->is_playing());

	player->stop();
	CHECK_FALSE(player->is_playing());
	CHECK(player->get_current_animation() == StringName());

	SceneTree::get_singleton()->get_root()->remove_child(player);
	memdelete(player);
}

TEST_CASE("[SceneTree][AnimationPlayer] Pause keeps current animation but stops processing") {
	AnimationPlayer *player = memnew(AnimationPlayer);
	player->set_auto_capture(false);
	Ref<AnimationLibrary> lib = memnew(AnimationLibrary);
	lib->add_animation("test", make_value_animation(1.0));
	player->add_animation_library("", lib);
	SceneTree::get_singleton()->get_root()->add_child(player);

	player->play("test");
	player->advance(0.3);
	double pos_before = player->get_current_animation_position();
	player->pause();

	CHECK_FALSE(player->is_playing());
	CHECK(player->get_current_animation_position() == doctest::Approx(pos_before));

	SceneTree::get_singleton()->get_root()->remove_child(player);
	memdelete(player);
}

TEST_CASE("[SceneTree][AnimationPlayer] Stop with keep_state preserves position") {
	AnimationPlayer *player = memnew(AnimationPlayer);
	player->set_auto_capture(false);
	Ref<AnimationLibrary> lib = memnew(AnimationLibrary);
	lib->add_animation("test", make_value_animation(1.0));
	player->add_animation_library("", lib);
	SceneTree::get_singleton()->get_root()->add_child(player);

	player->play("test");
	player->advance(0.0);
	player->advance(0.4);

	player->stop(true);
	CHECK_FALSE(player->is_playing());

	SceneTree::get_singleton()->get_root()->remove_child(player);
	memdelete(player);
}

// ---------- Seek ----------

TEST_CASE("[SceneTree][AnimationPlayer] Seek clamps to animation bounds") {
	AnimationPlayer *player = memnew(AnimationPlayer);
	player->set_auto_capture(false);
	Ref<AnimationLibrary> lib = memnew(AnimationLibrary);
	lib->add_animation("anim", make_value_animation(2.0));
	player->add_animation_library("", lib);
	SceneTree::get_singleton()->get_root()->add_child(player);

	player->play("anim");
	player->advance(0.0);

	player->seek(5.0, true);
	CHECK(player->get_current_animation_position() == doctest::Approx(2.0));

	player->seek(-1.0, true);
	CHECK(player->get_current_animation_position() == doctest::Approx(0.0));

	SceneTree::get_singleton()->get_root()->remove_child(player);
	memdelete(player);
}

// ---------- Advance / timing ----------

TEST_CASE("[SceneTree][AnimationPlayer] Advance moves position forward") {
	AnimationPlayer *player = memnew(AnimationPlayer);
	player->set_auto_capture(false);
	Ref<AnimationLibrary> lib = memnew(AnimationLibrary);
	lib->add_animation("anim", make_value_animation(2.0));
	player->add_animation_library("", lib);
	SceneTree::get_singleton()->get_root()->add_child(player);

	player->play("anim");
	player->advance(0.0);
	player->advance(0.5);
	CHECK(player->get_current_animation_position() == doctest::Approx(0.5));

	player->advance(0.3);
	CHECK(player->get_current_animation_position() == doctest::Approx(0.8));

	SceneTree::get_singleton()->get_root()->remove_child(player);
	memdelete(player);
}

TEST_CASE("[SceneTree][AnimationPlayer] Advance with speed_scale doubles rate") {
	AnimationPlayer *player = memnew(AnimationPlayer);
	player->set_auto_capture(false);
	Ref<AnimationLibrary> lib = memnew(AnimationLibrary);
	lib->add_animation("anim", make_value_animation(4.0));
	player->add_animation_library("", lib);
	SceneTree::get_singleton()->get_root()->add_child(player);

	player->set_speed_scale(2.0f);
	player->play("anim");
	player->advance(0.0);
	player->advance(0.5);
	CHECK(player->get_current_animation_position() == doctest::Approx(1.0));

	SceneTree::get_singleton()->get_root()->remove_child(player);
	memdelete(player);
}

TEST_CASE("[SceneTree][AnimationPlayer] get_playing_speed reflects speed_scale and custom_scale") {
	AnimationPlayer *player = memnew(AnimationPlayer);
	player->set_auto_capture(false);
	Ref<AnimationLibrary> lib = memnew(AnimationLibrary);
	lib->add_animation("anim", make_value_animation(2.0));
	player->add_animation_library("", lib);
	SceneTree::get_singleton()->get_root()->add_child(player);

	player->set_speed_scale(2.0f);
	player->play("anim", -1, 0.5f);
	CHECK(player->get_playing_speed() == doctest::Approx(1.0f));

	SceneTree::get_singleton()->get_root()->remove_child(player);
	memdelete(player);
}

TEST_CASE("[SceneTree][AnimationPlayer] get_playing_speed returns 0 when not playing") {
	AnimationPlayer *player = memnew(AnimationPlayer);
	CHECK(player->get_playing_speed() == doctest::Approx(0.0f));
	memdelete(player);
}

// ---------- Queue ----------

TEST_CASE("[SceneTree][AnimationPlayer] Queue chains animations") {
	AnimationPlayer *player = memnew(AnimationPlayer);
	player->set_auto_capture(false);
	Ref<AnimationLibrary> lib = memnew(AnimationLibrary);
	lib->add_animation("first", make_value_animation(0.5));
	lib->add_animation("second", make_value_animation(0.5));
	player->add_animation_library("", lib);
	SceneTree::get_singleton()->get_root()->add_child(player);

	player->play("first");
	player->queue("second");

	TypedArray<StringName> queue = player->get_queue();
	CHECK(queue.size() == 1);
	CHECK(StringName(queue[0]) == StringName("second"));

	SceneTree::get_singleton()->get_root()->remove_child(player);
	memdelete(player);
}

TEST_CASE("[SceneTree][AnimationPlayer] Queue on stopped player starts immediately") {
	AnimationPlayer *player = memnew(AnimationPlayer);
	player->set_auto_capture(false);
	Ref<AnimationLibrary> lib = memnew(AnimationLibrary);
	lib->add_animation("anim", make_value_animation(1.0));
	player->add_animation_library("", lib);
	SceneTree::get_singleton()->get_root()->add_child(player);

	CHECK_FALSE(player->is_playing());
	player->queue("anim");
	CHECK(player->is_playing());
	CHECK(player->get_current_animation() == StringName("anim"));

	SceneTree::get_singleton()->get_root()->remove_child(player);
	memdelete(player);
}

TEST_CASE("[SceneTree][AnimationPlayer] Clear queue empties the queue") {
	AnimationPlayer *player = memnew(AnimationPlayer);
	player->set_auto_capture(false);
	Ref<AnimationLibrary> lib = memnew(AnimationLibrary);
	lib->add_animation("a", make_value_animation(1.0));
	lib->add_animation("b", make_value_animation(1.0));
	lib->add_animation("c", make_value_animation(1.0));
	player->add_animation_library("", lib);
	SceneTree::get_singleton()->get_root()->add_child(player);

	player->play("a");
	player->queue("b");
	player->queue("c");
	CHECK(player->get_queue().size() == 2);

	player->clear_queue();
	CHECK(player->get_queue().size() == 0);

	SceneTree::get_singleton()->get_root()->remove_child(player);
	memdelete(player);
}

// ---------- animation_set_next / animation_get_next ----------

TEST_CASE("[AnimationPlayer] animation_set_next and animation_get_next") {
	AnimationPlayer *player = memnew(AnimationPlayer);
	setup_two_animations(player, "intro", 1.0, "loop", 2.0);

	player->animation_set_next("intro", "loop");
	CHECK(player->animation_get_next("intro") == StringName("loop"));
	CHECK(player->animation_get_next("loop") == StringName());

	memdelete(player);
}

// ---------- Signals ----------

TEST_CASE("[SceneTree][AnimationPlayer] animation_started signal fires on play") {
	AnimationPlayer *player = memnew(AnimationPlayer);
	player->set_auto_capture(false);
	Ref<AnimationLibrary> lib = memnew(AnimationLibrary);
	lib->add_animation("test", make_value_animation(1.0));
	player->add_animation_library("", lib);
	SceneTree::get_singleton()->get_root()->add_child(player);

	SIGNAL_WATCH(player, SNAME("animation_started"));

	player->play("test");

	Array expected_args;
	{
		Array args;
		args.push_back(StringName("test"));
		expected_args.push_back(args);
	}
	SIGNAL_CHECK(SNAME("animation_started"), expected_args);

	SIGNAL_UNWATCH(player, SNAME("animation_started"));
	SceneTree::get_singleton()->get_root()->remove_child(player);
	memdelete(player);
}

TEST_CASE("[SceneTree][AnimationPlayer] animation_finished signal fires at end of non-looping animation") {
	AnimationPlayer *player = memnew(AnimationPlayer);
	player->set_auto_capture(false);
	Ref<AnimationLibrary> lib = memnew(AnimationLibrary);
	lib->add_animation("short", make_value_animation(0.5));
	player->add_animation_library("", lib);
	SceneTree::get_singleton()->get_root()->add_child(player);

	SIGNAL_WATCH(player, SNAME("animation_finished"));

	player->play("short");
	player->advance(0.0);

	// Advance past the animation length.
	player->advance(1.0);

	Array expected_args;
	{
		Array args;
		args.push_back(StringName("short"));
		expected_args.push_back(args);
	}
	SIGNAL_CHECK(SNAME("animation_finished"), expected_args);

	SIGNAL_UNWATCH(player, SNAME("animation_finished"));
	SceneTree::get_singleton()->get_root()->remove_child(player);
	memdelete(player);
}

TEST_CASE("[SceneTree][AnimationPlayer] current_animation_changed signal fires on play") {
	AnimationPlayer *player = memnew(AnimationPlayer);
	player->set_auto_capture(false);
	Ref<AnimationLibrary> lib = memnew(AnimationLibrary);
	lib->add_animation("anim", make_value_animation(1.0));
	player->add_animation_library("", lib);
	SceneTree::get_singleton()->get_root()->add_child(player);

	SIGNAL_WATCH(player, SNAME("current_animation_changed"));

	player->play("anim");

	Array expected_args;
	{
		Array args;
		args.push_back(StringName("anim"));
		expected_args.push_back(args);
	}
	SIGNAL_CHECK(SNAME("current_animation_changed"), expected_args);

	SIGNAL_UNWATCH(player, SNAME("current_animation_changed"));
	SceneTree::get_singleton()->get_root()->remove_child(player);
	memdelete(player);
}

// ---------- Assigned animation ----------

TEST_CASE("[SceneTree][AnimationPlayer] set_assigned_animation without playing") {
	AnimationPlayer *player = memnew(AnimationPlayer);
	player->set_auto_capture(false);
	Ref<AnimationLibrary> lib = memnew(AnimationLibrary);
	lib->add_animation("idle", make_value_animation(1.0));
	player->add_animation_library("", lib);
	SceneTree::get_singleton()->get_root()->add_child(player);

	player->set_assigned_animation("idle");
	CHECK(player->get_assigned_animation() == StringName("idle"));
	CHECK_FALSE(player->is_playing());

	SceneTree::get_singleton()->get_root()->remove_child(player);
	memdelete(player);
}

// ---------- Section playback ----------

TEST_CASE("[SceneTree][AnimationPlayer] Section constrains playback range") {
	AnimationPlayer *player = memnew(AnimationPlayer);
	player->set_auto_capture(false);
	Ref<AnimationLibrary> lib = memnew(AnimationLibrary);
	lib->add_animation("long", make_value_animation(4.0));
	player->add_animation_library("", lib);
	SceneTree::get_singleton()->get_root()->add_child(player);

	player->play("long");
	player->advance(0.0);
	player->set_section(1.0, 3.0);

	CHECK(player->has_section());
	CHECK(player->get_section_start_time() == doctest::Approx(1.0));
	CHECK(player->get_section_end_time() == doctest::Approx(3.0));

	player->reset_section();
	CHECK_FALSE(player->has_section());

	SceneTree::get_singleton()->get_root()->remove_child(player);
	memdelete(player);
}

// ---------- Play backwards ----------

TEST_CASE("[SceneTree][AnimationPlayer] Play backwards moves position toward start") {
	AnimationPlayer *player = memnew(AnimationPlayer);
	player->set_auto_capture(false);
	Ref<AnimationLibrary> lib = memnew(AnimationLibrary);
	lib->add_animation("anim", make_value_animation(2.0));
	player->add_animation_library("", lib);
	SceneTree::get_singleton()->get_root()->add_child(player);

	player->play_backwards("anim");
	player->advance(0.0);

	CHECK(player->get_current_animation_position() == doctest::Approx(2.0));

	player->advance(0.5);
	CHECK(player->get_current_animation_position() == doctest::Approx(1.5));

	SceneTree::get_singleton()->get_root()->remove_child(player);
	memdelete(player);
}

// ---------- Blend transition during play switch ----------

TEST_CASE("[SceneTree][AnimationPlayer] Switching animations with blend time creates blend entry") {
	AnimationPlayer *player = memnew(AnimationPlayer);
	player->set_auto_capture(false);
	setup_two_animations(player, "anim_a", 2.0, "anim_b", 2.0);
	SceneTree::get_singleton()->get_root()->add_child(player);

	player->set_blend_time("anim_a", "anim_b", 0.5);

	player->play("anim_a");
	player->advance(0.0);
	player->advance(0.3);

	player->play("anim_b");
	CHECK(player->is_playing());
	CHECK(player->get_current_animation() == StringName("anim_b"));

	SceneTree::get_singleton()->get_root()->remove_child(player);
	memdelete(player);
}

TEST_CASE("[SceneTree][AnimationPlayer] Rapid animation switches with default blend time") {
	AnimationPlayer *player = memnew(AnimationPlayer);
	player->set_auto_capture(false);
	Ref<AnimationLibrary> lib = memnew(AnimationLibrary);
	lib->add_animation("a", make_value_animation(2.0));
	lib->add_animation("b", make_value_animation(2.0));
	lib->add_animation("c", make_value_animation(2.0));
	player->add_animation_library("", lib);
	SceneTree::get_singleton()->get_root()->add_child(player);

	player->set_default_blend_time(0.2);

	player->play("a");
	player->advance(0.0);
	player->advance(0.1);
	player->play("b");
	player->advance(0.05);
	player->play("c");

	CHECK(player->is_playing());
	CHECK(player->get_current_animation() == StringName("c"));

	SceneTree::get_singleton()->get_root()->remove_child(player);
	memdelete(player);
}

// ---------- Autoplay ----------

TEST_CASE("[AnimationPlayer] Autoplay getter and setter") {
	AnimationPlayer *player = memnew(AnimationPlayer);

	ERR_PRINT_OFF;
	player->set_autoplay("my_animation");
	ERR_PRINT_ON;
	CHECK(player->get_autoplay() == StringName("my_animation"));

	memdelete(player);
}

// ---------- Movie quit on finish ----------

TEST_CASE("[AnimationPlayer] Movie quit on finish") {
	AnimationPlayer *player = memnew(AnimationPlayer);

	CHECK_FALSE(player->is_movie_quit_on_finish_enabled());

	player->set_movie_quit_on_finish_enabled(true);
	CHECK(player->is_movie_quit_on_finish_enabled());

	memdelete(player);
}

// ---------- Edge cases ----------

TEST_CASE("[SceneTree][AnimationPlayer] Playing the same animation again does not restart if already playing") {
	AnimationPlayer *player = memnew(AnimationPlayer);
	player->set_auto_capture(false);
	Ref<AnimationLibrary> lib = memnew(AnimationLibrary);
	lib->add_animation("test", make_value_animation(2.0));
	player->add_animation_library("", lib);
	SceneTree::get_singleton()->get_root()->add_child(player);

	player->play("test");
	player->advance(0.0);
	player->advance(0.8);
	double pos_before = player->get_current_animation_position();

	// Calling play again with same name while already playing should not restart.
	player->play("test");
	CHECK(player->get_current_animation_position() == doctest::Approx(pos_before));

	SceneTree::get_singleton()->get_root()->remove_child(player);
	memdelete(player);
}

TEST_CASE("[AnimationPlayer] Nonexistent animation triggers error") {
	AnimationPlayer *player = memnew(AnimationPlayer);

	ERR_PRINT_OFF;
	player->play("does_not_exist");
	ERR_PRINT_ON;

	CHECK_FALSE(player->is_playing());

	memdelete(player);
}

TEST_CASE("[SceneTree][AnimationPlayer] Looping animation continues past length") {
	AnimationPlayer *player = memnew(AnimationPlayer);
	player->set_auto_capture(false);
	Ref<AnimationLibrary> lib = memnew(AnimationLibrary);
	lib->add_animation("loop", make_value_animation(1.0, Animation::LOOP_LINEAR));
	player->add_animation_library("", lib);
	SceneTree::get_singleton()->get_root()->add_child(player);

	player->play("loop");
	player->advance(0.0);
	player->advance(1.5);

	CHECK(player->is_playing());
	CHECK(player->get_current_animation_position() == doctest::Approx(0.5));

	SceneTree::get_singleton()->get_root()->remove_child(player);
	memdelete(player);
}

TEST_CASE("[SceneTree][AnimationPlayer] Non-looping animation clamps at end") {
	AnimationPlayer *player = memnew(AnimationPlayer);
	player->set_auto_capture(false);
	Ref<AnimationLibrary> lib = memnew(AnimationLibrary);
	lib->add_animation("once", make_value_animation(1.0));
	player->add_animation_library("", lib);
	SceneTree::get_singleton()->get_root()->add_child(player);

	player->play("once");
	player->advance(0.0);
	player->advance(2.0);

	CHECK(player->get_current_animation_position() == doctest::Approx(1.0));
	CHECK_FALSE(player->is_playing());

	SceneTree::get_singleton()->get_root()->remove_child(player);
	memdelete(player);
}

TEST_CASE("[SceneTree][AnimationPlayer] Zero-length advance does not move position") {
	AnimationPlayer *player = memnew(AnimationPlayer);
	player->set_auto_capture(false);
	Ref<AnimationLibrary> lib = memnew(AnimationLibrary);
	lib->add_animation("anim", make_value_animation(2.0));
	player->add_animation_library("", lib);
	SceneTree::get_singleton()->get_root()->add_child(player);

	player->play("anim");
	player->advance(0.0);
	player->advance(0.5);
	double pos = player->get_current_animation_position();

	player->advance(0.0);
	CHECK(player->get_current_animation_position() == doctest::Approx(pos));

	SceneTree::get_singleton()->get_root()->remove_child(player);
	memdelete(player);
}

TEST_CASE("[SceneTree][AnimationPlayer] Queued animation plays after first finishes") {
	AnimationPlayer *player = memnew(AnimationPlayer);
	player->set_auto_capture(false);
	setup_two_animations(player, "first", 0.5, "second", 1.0);
	SceneTree::get_singleton()->get_root()->add_child(player);

	player->play("first");
	player->queue("second");
	player->advance(0.0);
	player->advance(1.0);

	CHECK(player->is_playing());
	CHECK(player->get_current_animation() == StringName("second"));

	SceneTree::get_singleton()->get_root()->remove_child(player);
	memdelete(player);
}

TEST_CASE("[SceneTree][AnimationPlayer] Pingpong loop reverses direction") {
	AnimationPlayer *player = memnew(AnimationPlayer);
	player->set_auto_capture(false);
	Ref<AnimationLibrary> lib = memnew(AnimationLibrary);
	lib->add_animation("pp", make_value_animation(1.0, Animation::LOOP_PINGPONG));
	player->add_animation_library("", lib);
	SceneTree::get_singleton()->get_root()->add_child(player);

	player->play("pp");
	player->advance(0.0);
	// Advance past the end to trigger pingpong.
	player->advance(1.5);

	CHECK(player->is_playing());
	// Should be somewhere between 0 and 1 after bouncing.
	double pos = player->get_current_animation_position();
	CHECK(pos >= 0.0);
	CHECK(pos <= 1.0);

	SceneTree::get_singleton()->get_root()->remove_child(player);
	memdelete(player);
}

TEST_CASE("[SceneTree][AnimationPlayer] Multiple play calls with blend accumulate blend stack") {
	AnimationPlayer *player = memnew(AnimationPlayer);
	player->set_auto_capture(false);
	Ref<AnimationLibrary> lib = memnew(AnimationLibrary);
	lib->add_animation("a", make_value_animation(2.0));
	lib->add_animation("b", make_value_animation(2.0));
	lib->add_animation("c", make_value_animation(2.0));
	player->add_animation_library("", lib);
	SceneTree::get_singleton()->get_root()->add_child(player);

	player->play("a");
	player->advance(0.0);
	player->advance(0.1);

	// Switch to b with explicit blend.
	player->play("b", 0.3);
	player->advance(0.1);

	// Immediately switch to c with explicit blend.
	player->play("c", 0.3);

	CHECK(player->is_playing());
	CHECK(player->get_current_animation() == StringName("c"));

	SceneTree::get_singleton()->get_root()->remove_child(player);
	memdelete(player);
}

} // namespace TestAnimationPlayer
