/**************************************************************************/
/*  test_animation_tree.cpp                                               */
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

TEST_FORCE_LINK(test_animation_tree)

#include "scene/animation/animation_blend_tree.h"
#include "scene/animation/animation_node_state_machine.h"
#include "scene/animation/animation_player.h"
#include "scene/animation/animation_tree.h"
#include "scene/main/scene_tree.h"
#include "scene/main/window.h"
#include "scene/resources/animation.h"
#include "scene/resources/animation_library.h"
#include "tests/signal_watcher.h"

namespace TestAnimationTree {

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

// Helper: set up a minimal AnimationTree scene with an AnimationPlayer sibling
// and a BlendTree root containing given animation node(s).
// Returns the AnimationTree pointer. Caller must remove/memdelete both nodes.
struct TreeSetup {
	AnimationPlayer *player = nullptr;
	AnimationTree *tree = nullptr;
	Ref<AnimationNodeBlendTree> blend_tree;
};

static TreeSetup make_tree_with_player(const HashMap<StringName, Ref<Animation>> &p_anims) {
	TreeSetup s;

	// Root scene node.
	Node *root = SceneTree::get_singleton()->get_root();

	// AnimationPlayer as child of root.
	s.player = memnew(AnimationPlayer);
	s.player->set_auto_capture(false);
	root->add_child(s.player);

	// Add animations to a default library.
	Ref<AnimationLibrary> lib = memnew(AnimationLibrary);
	for (const KeyValue<StringName, Ref<Animation>> &kv : p_anims) {
		lib->add_animation(kv.key, kv.value);
	}
	s.player->add_animation_library("", lib);

	// AnimationTree as sibling.
	s.tree = memnew(AnimationTree);
	root->add_child(s.tree);

	// Create a BlendTree root node.
	s.blend_tree.instantiate();
	s.tree->set_root_animation_node(s.blend_tree);

	// Point the tree to the player.
	s.tree->set_animation_player(s.tree->get_path_to(s.player));

	return s;
}

static void cleanup_tree_setup(TreeSetup &s) {
	Node *root = SceneTree::get_singleton()->get_root();
	root->remove_child(s.tree);
	root->remove_child(s.player);
	memdelete(s.tree);
	memdelete(s.player);
}

// ---------- Basic AnimationTree setup ----------

TEST_CASE("[SceneTree][AnimationTree] Root animation node getter and setter") {
	AnimationTree *tree = memnew(AnimationTree);

	CHECK(tree->get_root_animation_node().is_null());

	Ref<AnimationNodeBlendTree> bt;
	bt.instantiate();
	tree->set_root_animation_node(bt);
	CHECK(tree->get_root_animation_node() == bt);

	tree->set_root_animation_node(Ref<AnimationRootNode>());
	CHECK(tree->get_root_animation_node().is_null());

	memdelete(tree);
}

TEST_CASE("[SceneTree][AnimationTree] Animation player path getter and setter") {
	AnimationTree *tree = memnew(AnimationTree);
	SceneTree::get_singleton()->get_root()->add_child(tree);

	CHECK(tree->get_animation_player() == NodePath());

	AnimationPlayer *player = memnew(AnimationPlayer);
	SceneTree::get_singleton()->get_root()->add_child(player);
	tree->set_animation_player(tree->get_path_to(player));
	CHECK(tree->get_animation_player() == tree->get_path_to(player));

	SceneTree::get_singleton()->get_root()->remove_child(tree);
	SceneTree::get_singleton()->get_root()->remove_child(player);
	memdelete(tree);
	memdelete(player);
}

TEST_CASE("[SceneTree][AnimationTree] Advance expression base node") {
	AnimationTree *tree = memnew(AnimationTree);

	CHECK(tree->get_advance_expression_base_node() == NodePath("."));

	tree->set_advance_expression_base_node(NodePath(".."));
	CHECK(tree->get_advance_expression_base_node() == NodePath(".."));

	memdelete(tree);
}

// ---------- AnimationNodeBlendTree node management ----------

TEST_CASE("[SceneTree][AnimationTree] BlendTree add, remove, and rename nodes") {
	Ref<AnimationNodeBlendTree> bt;
	bt.instantiate();

	CHECK(bt->has_node("output"));
	CHECK_EQ(bt->get_node_list().size(), 1);

	Ref<AnimationNodeAnimation> anim_node;
	anim_node.instantiate();
	anim_node->set_animation("walk");

	bt->add_node("walk_node", anim_node, Vector2(100, 0));
	CHECK(bt->has_node("walk_node"));
	CHECK_EQ(bt->get_node("walk_node"), anim_node);
	CHECK_EQ(bt->get_node_position("walk_node"), Vector2(100, 0));

	bt->rename_node("walk_node", "run_node");
	CHECK_FALSE(bt->has_node("walk_node"));
	CHECK(bt->has_node("run_node"));

	bt->remove_node("run_node");
	CHECK_FALSE(bt->has_node("run_node"));
	CHECK_EQ(bt->get_node_list().size(), 1);
}

TEST_CASE("[SceneTree][AnimationTree] BlendTree node connections") {
	Ref<AnimationNodeBlendTree> bt;
	bt.instantiate();

	Ref<AnimationNodeAnimation> anim1;
	anim1.instantiate();
	anim1->set_animation("anim1");
	bt->add_node("anim1", anim1);

	Ref<AnimationNodeAnimation> anim2;
	anim2.instantiate();
	anim2->set_animation("anim2");
	bt->add_node("anim2", anim2);

	Ref<AnimationNodeBlend2> blend2;
	blend2.instantiate();
	bt->add_node("blend", blend2);

	CHECK_EQ(bt->can_connect_node("blend", 0, "anim1"), AnimationNodeBlendTree::CONNECTION_OK);
	bt->connect_node("blend", 0, "anim1");
	CHECK_EQ(bt->can_connect_node("blend", 1, "anim2"), AnimationNodeBlendTree::CONNECTION_OK);
	bt->connect_node("blend", 1, "anim2");

	const LocalVector<StringName> *conns = bt->get_node_connection_array("blend");
	CHECK_EQ(conns->size(), 2);
	CHECK_EQ((*conns)[0], StringName("anim1"));
	CHECK_EQ((*conns)[1], StringName("anim2"));

	bt->disconnect_node("blend", 0);
	conns = bt->get_node_connection_array("blend");
	CHECK_EQ((*conns)[0], StringName());
	CHECK_EQ((*conns)[1], StringName("anim2"));
}

TEST_CASE("[SceneTree][AnimationTree] BlendTree self-connection is rejected") {
	Ref<AnimationNodeBlendTree> bt;
	bt.instantiate();

	Ref<AnimationNodeAnimation> anim_node;
	anim_node.instantiate();
	bt->add_node("node", anim_node);

	CHECK_EQ(bt->can_connect_node("node", 0, "node"), AnimationNodeBlendTree::CONNECTION_ERROR_SAME_NODE);
}

TEST_CASE("[SceneTree][AnimationTree] BlendTree connect to nonexistent input index fails") {
	Ref<AnimationNodeBlendTree> bt;
	bt.instantiate();

	Ref<AnimationNodeAnimation> anim_node;
	anim_node.instantiate();
	bt->add_node("node", anim_node);

	CHECK_EQ(bt->can_connect_node("output", 5, "node"), AnimationNodeBlendTree::CONNECTION_ERROR_NO_INPUT_INDEX);
}

// ---------- AnimationNode input management ----------

TEST_CASE("[AnimationTree] AnimationNode input add and remove") {
	Ref<AnimationNodeBlend2> blend2;
	blend2.instantiate();

	int initial_count = blend2->get_input_count();
	CHECK(initial_count == 2);

	CHECK(blend2->get_input_name(0) == "in");
	CHECK(blend2->get_input_name(1) == "blend");
}

TEST_CASE("[AnimationTree] AnimationNode filter path") {
	Ref<AnimationNodeBlend2> blend2;
	blend2.instantiate();

	blend2->set_filter_path(NodePath("Skeleton3D:Arm"), true);
	CHECK(blend2->is_path_filtered(NodePath("Skeleton3D:Arm")));
	CHECK_FALSE(blend2->is_path_filtered(NodePath("Skeleton3D:Leg")));

	blend2->set_filter_path(NodePath("Skeleton3D:Arm"), false);
	CHECK_FALSE(blend2->is_path_filtered(NodePath("Skeleton3D:Arm")));
}

TEST_CASE("[AnimationTree] AnimationNode filter enable/disable") {
	Ref<AnimationNodeBlend2> blend2;
	blend2.instantiate();

	CHECK_FALSE(blend2->is_filter_enabled());
	blend2->set_filter_enabled(true);
	CHECK(blend2->is_filter_enabled());
	blend2->set_filter_enabled(false);
	CHECK_FALSE(blend2->is_filter_enabled());
}

// ---------- AnimationNodeAnimation ----------

TEST_CASE("[AnimationTree] AnimationNodeAnimation getters and setters") {
	Ref<AnimationNodeAnimation> anim;
	anim.instantiate();

	anim->set_animation("walk");
	CHECK(anim->get_animation() == StringName("walk"));

	anim->set_play_mode(AnimationNodeAnimation::PLAY_MODE_BACKWARD);
	CHECK(anim->get_play_mode() == AnimationNodeAnimation::PLAY_MODE_BACKWARD);

	anim->set_play_mode(AnimationNodeAnimation::PLAY_MODE_FORWARD);
	CHECK(anim->get_play_mode() == AnimationNodeAnimation::PLAY_MODE_FORWARD);
}

TEST_CASE("[AnimationTree] AnimationNodeAnimation custom timeline") {
	Ref<AnimationNodeAnimation> anim;
	anim.instantiate();

	CHECK_FALSE(anim->is_using_custom_timeline());

	anim->set_use_custom_timeline(true);
	CHECK(anim->is_using_custom_timeline());

	anim->set_timeline_length(2.5);
	CHECK(anim->get_timeline_length() == doctest::Approx(2.5));

	anim->set_loop_mode(Animation::LOOP_LINEAR);
	CHECK(anim->get_loop_mode() == Animation::LOOP_LINEAR);

	anim->set_stretch_time_scale(false);
	CHECK_FALSE(anim->is_stretching_time_scale());

	anim->set_start_offset(0.5);
	CHECK(anim->get_start_offset() == doctest::Approx(0.5));
}

TEST_CASE("[AnimationTree] AnimationNodeAnimation advance_on_start") {
	Ref<AnimationNodeAnimation> anim;
	anim.instantiate();

	CHECK_FALSE(anim->is_advance_on_start());
	anim->set_advance_on_start(true);
	CHECK(anim->is_advance_on_start());
}

// ---------- Blend node types ----------

TEST_CASE("[AnimationTree] AnimationNodeBlend2 has correct input count and caption") {
	Ref<AnimationNodeBlend2> blend;
	blend.instantiate();
	CHECK(blend->get_input_count() == 2);
	CHECK(blend->has_filter());
	CHECK_FALSE(blend->get_caption().is_empty());
}

TEST_CASE("[AnimationTree] AnimationNodeBlend3 has correct input count") {
	Ref<AnimationNodeBlend3> blend;
	blend.instantiate();
	CHECK(blend->get_input_count() == 3);
}

TEST_CASE("[AnimationTree] AnimationNodeAdd2 has correct input count and caption") {
	Ref<AnimationNodeAdd2> add;
	add.instantiate();
	CHECK(add->get_input_count() == 2);
	CHECK(add->has_filter());
	CHECK_FALSE(add->get_caption().is_empty());
}

TEST_CASE("[AnimationTree] AnimationNodeAdd3 has correct input count") {
	Ref<AnimationNodeAdd3> add;
	add.instantiate();
	CHECK(add->get_input_count() == 3);
}

TEST_CASE("[AnimationTree] AnimationNodeSub2 has correct input count and caption") {
	Ref<AnimationNodeSub2> sub;
	sub.instantiate();
	CHECK(sub->get_input_count() == 2);
	CHECK(sub->has_filter());
	CHECK_FALSE(sub->get_caption().is_empty());
}

// ---------- AnimationNodeTimeScale ----------

TEST_CASE("[AnimationTree] AnimationNodeTimeScale defaults") {
	Ref<AnimationNodeTimeScale> ts;
	ts.instantiate();
	CHECK(ts->get_input_count() == 1);

	Variant def = ts->get_parameter_default_value("scale");
	CHECK(double(def) == doctest::Approx(1.0));
}

// ---------- AnimationNodeTimeSeek ----------

TEST_CASE("[AnimationTree] AnimationNodeTimeSeek defaults and explicit elapse") {
	Ref<AnimationNodeTimeSeek> seek;
	seek.instantiate();
	CHECK(seek->get_input_count() == 1);

	Variant def = seek->get_parameter_default_value("seek_request");
	CHECK(double(def) == doctest::Approx(-1.0));

	CHECK(seek->is_explicit_elapse());
	seek->set_explicit_elapse(false);
	CHECK_FALSE(seek->is_explicit_elapse());
}

// ---------- AnimationNodeOneShot ----------

TEST_CASE("[AnimationTree] AnimationNodeOneShot fade times and properties") {
	Ref<AnimationNodeOneShot> oneshot;
	oneshot.instantiate();

	CHECK(oneshot->get_input_count() == 2);

	oneshot->set_fade_in_time(0.3);
	CHECK(oneshot->get_fade_in_time() == doctest::Approx(0.3));

	oneshot->set_fade_out_time(0.5);
	CHECK(oneshot->get_fade_out_time() == doctest::Approx(0.5));

	oneshot->set_mix_mode(AnimationNodeOneShot::MIX_MODE_ADD);
	CHECK(oneshot->get_mix_mode() == AnimationNodeOneShot::MIX_MODE_ADD);

	oneshot->set_break_loop_at_end(true);
	CHECK(oneshot->is_loop_broken_at_end());

	oneshot->set_abort_on_reset(true);
	CHECK(oneshot->is_aborted_on_reset());

	CHECK(oneshot->has_filter());
}

TEST_CASE("[AnimationTree] AnimationNodeOneShot auto restart") {
	Ref<AnimationNodeOneShot> oneshot;
	oneshot.instantiate();

	CHECK_FALSE(oneshot->is_auto_restart_enabled());
	oneshot->set_auto_restart_enabled(true);
	CHECK(oneshot->is_auto_restart_enabled());

	oneshot->set_auto_restart_delay(2.0);
	CHECK(oneshot->get_auto_restart_delay() == doctest::Approx(2.0));

	oneshot->set_auto_restart_random_delay(0.5);
	CHECK(oneshot->get_auto_restart_random_delay() == doctest::Approx(0.5));
}

TEST_CASE("[AnimationTree] AnimationNodeOneShot fade in/out curves") {
	Ref<AnimationNodeOneShot> oneshot;
	oneshot.instantiate();

	CHECK(oneshot->get_fade_in_curve().is_null());
	CHECK(oneshot->get_fade_out_curve().is_null());

	Ref<Curve> curve;
	curve.instantiate();
	oneshot->set_fade_in_curve(curve);
	CHECK(oneshot->get_fade_in_curve() == curve);

	Ref<Curve> curve2;
	curve2.instantiate();
	oneshot->set_fade_out_curve(curve2);
	CHECK(oneshot->get_fade_out_curve() == curve2);
}

// ---------- AnimationNodeTransition ----------

TEST_CASE("[AnimationTree] AnimationNodeTransition input management") {
	Ref<AnimationNodeTransition> transition;
	transition.instantiate();

	int default_count = transition->get_input_count();
	CHECK(default_count >= 0);

	transition->set_input_count(3);
	CHECK(transition->get_input_count() == 3);

	transition->set_input_name(0, "idle");
	CHECK(transition->get_input_name(0) == "idle");
	transition->set_input_name(1, "walk");
	CHECK(transition->get_input_name(1) == "walk");
	transition->set_input_name(2, "run");
	CHECK(transition->get_input_name(2) == "run");
}

TEST_CASE("[AnimationTree] AnimationNodeTransition xfade time and curve") {
	Ref<AnimationNodeTransition> transition;
	transition.instantiate();

	transition->set_xfade_time(0.4);
	CHECK(transition->get_xfade_time() == doctest::Approx(0.4));

	CHECK(transition->get_xfade_curve().is_null());
	Ref<Curve> curve;
	curve.instantiate();
	transition->set_xfade_curve(curve);
	CHECK(transition->get_xfade_curve() == curve);
}

TEST_CASE("[AnimationTree] AnimationNodeTransition auto advance and reset per input") {
	Ref<AnimationNodeTransition> transition;
	transition.instantiate();
	transition->set_input_count(2);
	transition->set_input_name(0, "state_a");
	transition->set_input_name(1, "state_b");

	CHECK_FALSE(transition->is_input_set_as_auto_advance(0));
	transition->set_input_as_auto_advance(0, true);
	CHECK(transition->is_input_set_as_auto_advance(0));

	CHECK(transition->is_input_reset(0));
	transition->set_input_reset(0, false);
	CHECK_FALSE(transition->is_input_reset(0));
}

TEST_CASE("[AnimationTree] AnimationNodeTransition allow self-transition") {
	Ref<AnimationNodeTransition> transition;
	transition.instantiate();

	CHECK_FALSE(transition->is_allow_transition_to_self());
	transition->set_allow_transition_to_self(true);
	CHECK(transition->is_allow_transition_to_self());
}

TEST_CASE("[AnimationTree] AnimationNodeTransition break loop at end") {
	Ref<AnimationNodeTransition> transition;
	transition.instantiate();
	transition->set_input_count(2);

	CHECK_FALSE(transition->is_input_loop_broken_at_end(0));
	transition->set_input_break_loop_at_end(0, true);
	CHECK(transition->is_input_loop_broken_at_end(0));
}

// ---------- AnimationNodeSync ----------

TEST_CASE("[AnimationTree] AnimationNodeSync use_sync flag") {
	Ref<AnimationNodeBlend2> blend;
	blend.instantiate();

	CHECK_FALSE(blend->is_using_sync());
	blend->set_use_sync(true);
	CHECK(blend->is_using_sync());
}

// ---------- BlendTree graph offset ----------

TEST_CASE("[AnimationTree] BlendTree graph offset") {
	Ref<AnimationNodeBlendTree> bt;
	bt.instantiate();

	CHECK_EQ(bt->get_graph_offset(), Vector2(0, 0));

	bt->set_graph_offset(Vector2(200, 100));
	CHECK_EQ(bt->get_graph_offset(), Vector2(200, 100));
}

// ---------- AnimationTree active state ----------

TEST_CASE("[SceneTree][AnimationTree] Active state getter and setter") {
	AnimationTree *tree = memnew(AnimationTree);
	SceneTree::get_singleton()->get_root()->add_child(tree);

	CHECK(tree->is_active());

	tree->set_active(false);
	CHECK_FALSE(tree->is_active());

	tree->set_active(true);
	CHECK(tree->is_active());

	SceneTree::get_singleton()->get_root()->remove_child(tree);
	memdelete(tree);
}

// ---------- AnimationTree callback mode ----------

TEST_CASE("[AnimationTree] Callback mode process") {
	AnimationTree *tree = memnew(AnimationTree);

	tree->set_callback_mode_process(AnimationMixer::ANIMATION_CALLBACK_MODE_PROCESS_MANUAL);
	CHECK(tree->get_callback_mode_process() == AnimationMixer::ANIMATION_CALLBACK_MODE_PROCESS_MANUAL);

	tree->set_callback_mode_process(AnimationMixer::ANIMATION_CALLBACK_MODE_PROCESS_PHYSICS);
	CHECK(tree->get_callback_mode_process() == AnimationMixer::ANIMATION_CALLBACK_MODE_PROCESS_PHYSICS);

	memdelete(tree);
}

TEST_CASE("[AnimationTree] Callback mode method") {
	AnimationTree *tree = memnew(AnimationTree);

	tree->set_callback_mode_method(AnimationMixer::ANIMATION_CALLBACK_MODE_METHOD_IMMEDIATE);
	CHECK(tree->get_callback_mode_method() == AnimationMixer::ANIMATION_CALLBACK_MODE_METHOD_IMMEDIATE);

	memdelete(tree);
}

// ---------- AnimationTree with AnimationPlayer integration ----------

TEST_CASE("[SceneTree][AnimationTree] Tree inherits libraries from linked AnimationPlayer") {
	HashMap<StringName, Ref<Animation>> anims;
	anims.insert("walk", make_value_animation(1.0));
	anims.insert("run", make_value_animation(0.8));

	TreeSetup s = make_tree_with_player(anims);

	CHECK(s.tree->has_animation("walk"));
	CHECK(s.tree->has_animation("run"));

	cleanup_tree_setup(s);
}

TEST_CASE("[SceneTree][AnimationTree] Clearing animation player path removes libraries") {
	HashMap<StringName, Ref<Animation>> anims;
	anims.insert("anim", make_value_animation(1.0));

	TreeSetup s = make_tree_with_player(anims);
	CHECK(s.tree->has_animation("anim"));

	s.tree->set_animation_player(NodePath());
	CHECK_FALSE(s.tree->has_animation("anim"));

	cleanup_tree_setup(s);
}

TEST_CASE("[SceneTree][AnimationTree] animation_player_changed signal fires on set") {
	AnimationTree *tree = memnew(AnimationTree);
	SceneTree::get_singleton()->get_root()->add_child(tree);

	AnimationPlayer *player = memnew(AnimationPlayer);
	SceneTree::get_singleton()->get_root()->add_child(player);

	SIGNAL_WATCH(tree, SNAME("animation_player_changed"));

	tree->set_animation_player(tree->get_path_to(player));

	Array expected;
	expected.push_back(Array());
	SIGNAL_CHECK(SNAME("animation_player_changed"), expected);

	SIGNAL_UNWATCH(tree, SNAME("animation_player_changed"));
	SceneTree::get_singleton()->get_root()->remove_child(tree);
	SceneTree::get_singleton()->get_root()->remove_child(player);
	memdelete(tree);
	memdelete(player);
}

// ---------- Complex BlendTree topology ----------

TEST_CASE("[SceneTree][AnimationTree] BlendTree with Blend2 connected to output") {
	Ref<AnimationNodeBlendTree> bt;
	bt.instantiate();

	Ref<AnimationNodeAnimation> anim_a;
	anim_a.instantiate();
	anim_a->set_animation("walk");

	Ref<AnimationNodeAnimation> anim_b;
	anim_b.instantiate();
	anim_b->set_animation("run");

	Ref<AnimationNodeBlend2> blend2;
	blend2.instantiate();

	bt->add_node("walk", anim_a);
	bt->add_node("run", anim_b);
	bt->add_node("blend", blend2);

	bt->connect_node("blend", 0, "walk");
	bt->connect_node("blend", 1, "run");
	bt->connect_node("output", 0, "blend");

	const LocalVector<StringName> *output_conns = bt->get_node_connection_array("output");
	CHECK_EQ(output_conns->size(), 1);
	CHECK_EQ((*output_conns)[0], StringName("blend"));

	const LocalVector<StringName> *blend_conns = bt->get_node_connection_array("blend");
	CHECK_EQ(blend_conns->size(), 2);
	CHECK_EQ((*blend_conns)[0], StringName("walk"));
	CHECK_EQ((*blend_conns)[1], StringName("run"));
}

TEST_CASE("[SceneTree][AnimationTree] BlendTree rename preserves connections") {
	Ref<AnimationNodeBlendTree> bt;
	bt.instantiate();

	Ref<AnimationNodeAnimation> anim;
	anim.instantiate();
	bt->add_node("old_name", anim);
	bt->connect_node("output", 0, "old_name");

	bt->rename_node("old_name", "new_name");

	const LocalVector<StringName> *conns = bt->get_node_connection_array("output");
	CHECK_EQ((*conns)[0], StringName("new_name"));
}

TEST_CASE("[SceneTree][AnimationTree] BlendTree remove clears connections") {
	Ref<AnimationNodeBlendTree> bt;
	bt.instantiate();

	Ref<AnimationNodeAnimation> anim;
	anim.instantiate();
	bt->add_node("node", anim);
	bt->connect_node("output", 0, "node");

	bt->remove_node("node");

	const LocalVector<StringName> *conns = bt->get_node_connection_array("output");
	CHECK_EQ((*conns)[0], StringName());
}

// ---------- AnimationNodeTransition input add/remove ----------

TEST_CASE("[AnimationTree] AnimationNodeTransition dynamic input management") {
	Ref<AnimationNodeTransition> transition;
	transition.instantiate();

	transition->set_input_count(0);

	transition->add_input("state_a");
	transition->add_input("state_b");
	CHECK(transition->get_input_count() == 2);
	CHECK(transition->get_input_name(0) == "state_a");
	CHECK(transition->get_input_name(1) == "state_b");

	transition->remove_input(0);
	CHECK(transition->get_input_count() == 1);
	CHECK(transition->get_input_name(0) == "state_b");
}

// ---------- AnimationNode deletable flag ----------

TEST_CASE("[AnimationTree] AnimationNode deletable flag") {
	Ref<AnimationNodeAnimation> anim;
	anim.instantiate();

	anim->set_deletable(true);
	CHECK(anim->is_deletable());

	anim->set_deletable(false);
	CHECK_FALSE(anim->is_deletable());
}

// ---------- AnimationTree deterministic mode ----------

TEST_CASE("[AnimationTree] Deterministic mode default") {
	AnimationTree *tree = memnew(AnimationTree);

	CHECK(tree->is_deterministic());

	memdelete(tree);
}

// ---------- AnimationTree root motion ----------

TEST_CASE("[AnimationTree] Root motion track getter and setter") {
	AnimationTree *tree = memnew(AnimationTree);

	tree->set_root_motion_track(NodePath("Skeleton3D:Hips"));
	CHECK(tree->get_root_motion_track() == NodePath("Skeleton3D:Hips"));

	tree->set_root_motion_local(true);
	CHECK(tree->is_root_motion_local());

	memdelete(tree);
}

// ---------- AnimationTree audio polyphony ----------

TEST_CASE("[AnimationTree] Audio max polyphony") {
	AnimationTree *tree = memnew(AnimationTree);

	tree->set_audio_max_polyphony(16);
	CHECK(tree->get_audio_max_polyphony() == 16);

	memdelete(tree);
}

// ---------- AnimationNodeBlendTree duplicate node name ----------

TEST_CASE("[SceneTree][AnimationTree] BlendTree adding duplicate name errors") {
	Ref<AnimationNodeBlendTree> bt;
	bt.instantiate();

	Ref<AnimationNodeAnimation> anim1;
	anim1.instantiate();
	bt->add_node("node", anim1);

	Ref<AnimationNodeAnimation> anim2;
	anim2.instantiate();
	ERR_PRINT_OFF;
	bt->add_node("node", anim2);
	ERR_PRINT_ON;

	CHECK(bt->get_node("node") == anim1);
}

// ---------- AnimationTree state validation (no player connected) ----------

TEST_CASE("[SceneTree][AnimationTree] Tree without player is invalid state") {
	AnimationTree *tree = memnew(AnimationTree);
	Ref<AnimationNodeBlendTree> bt;
	bt.instantiate();
	tree->set_root_animation_node(bt);

	SceneTree::get_singleton()->get_root()->add_child(tree);

	tree->set_callback_mode_process(AnimationMixer::ANIMATION_CALLBACK_MODE_PROCESS_MANUAL);
	tree->advance(0.0);

	SceneTree::get_singleton()->get_root()->remove_child(tree);
	memdelete(tree);
}

// ---------- AnimationNodeTransition find_input ----------

TEST_CASE("[AnimationTree] AnimationNodeTransition find_input") {
	Ref<AnimationNodeTransition> transition;
	transition.instantiate();
	transition->set_input_count(3);
	transition->set_input_name(0, "idle");
	transition->set_input_name(1, "walk");
	transition->set_input_name(2, "run");

	CHECK(transition->find_input("walk") == 1);
	CHECK(transition->find_input("run") == 2);
	CHECK(transition->find_input("nonexistent") == -1);
}

// ---------- AnimationTree with manual process ----------

TEST_CASE("[SceneTree][AnimationTree] Manual advance with simple blend tree") {
	HashMap<StringName, Ref<Animation>> anims;
	anims.insert("idle", make_value_animation(2.0));

	TreeSetup s = make_tree_with_player(anims);

	Ref<AnimationNodeAnimation> anim_node;
	anim_node.instantiate();
	anim_node->set_animation("idle");
	s.blend_tree->add_node("idle_anim", anim_node);
	s.blend_tree->connect_node("output", 0, "idle_anim");

	s.tree->set_callback_mode_process(AnimationMixer::ANIMATION_CALLBACK_MODE_PROCESS_MANUAL);
	s.tree->advance(0.0);
	s.tree->advance(0.5);

	cleanup_tree_setup(s);
}

TEST_CASE("[SceneTree][AnimationTree] Advance processes without crash when blend tree is empty") {
	HashMap<StringName, Ref<Animation>> anims;
	anims.insert("anim", make_value_animation(1.0));

	TreeSetup s = make_tree_with_player(anims);

	s.tree->set_callback_mode_process(AnimationMixer::ANIMATION_CALLBACK_MODE_PROCESS_MANUAL);
	s.tree->advance(0.0);
	s.tree->advance(0.1);

	cleanup_tree_setup(s);
}

// ---------- AnimationTree with Blend2 blending ----------

TEST_CASE("[SceneTree][AnimationTree] Blend2 with two animations processes without crash") {
	HashMap<StringName, Ref<Animation>> anims;
	anims.insert("walk", make_value_animation(1.0, Animation::LOOP_LINEAR));
	anims.insert("run", make_value_animation(0.8, Animation::LOOP_LINEAR));

	TreeSetup s = make_tree_with_player(anims);

	Ref<AnimationNodeAnimation> walk_node;
	walk_node.instantiate();
	walk_node->set_animation("walk");
	s.blend_tree->add_node("walk", walk_node);

	Ref<AnimationNodeAnimation> run_node;
	run_node.instantiate();
	run_node->set_animation("run");
	s.blend_tree->add_node("run", run_node);

	Ref<AnimationNodeBlend2> blend2;
	blend2.instantiate();
	s.blend_tree->add_node("blend", blend2);

	s.blend_tree->connect_node("blend", 0, "walk");
	s.blend_tree->connect_node("blend", 1, "run");
	s.blend_tree->connect_node("output", 0, "blend");

	s.tree->set_callback_mode_process(AnimationMixer::ANIMATION_CALLBACK_MODE_PROCESS_MANUAL);

	// Process several frames with different blend amounts.
	s.tree->set("parameters/blend/blend_amount", 0.0);
	s.tree->advance(0.0);
	s.tree->advance(0.1);

	s.tree->set("parameters/blend/blend_amount", 0.5);
	s.tree->advance(0.1);

	s.tree->set("parameters/blend/blend_amount", 1.0);
	s.tree->advance(0.1);

	cleanup_tree_setup(s);
}

// ---------- AnimationTree with TimeScale ----------

TEST_CASE("[SceneTree][AnimationTree] TimeScale node scales playback rate") {
	HashMap<StringName, Ref<Animation>> anims;
	anims.insert("anim", make_value_animation(2.0));

	TreeSetup s = make_tree_with_player(anims);

	Ref<AnimationNodeAnimation> anim_node;
	anim_node.instantiate();
	anim_node->set_animation("anim");
	s.blend_tree->add_node("anim", anim_node);

	Ref<AnimationNodeTimeScale> ts;
	ts.instantiate();
	s.blend_tree->add_node("timescale", ts);

	s.blend_tree->connect_node("timescale", 0, "anim");
	s.blend_tree->connect_node("output", 0, "timescale");

	s.tree->set_callback_mode_process(AnimationMixer::ANIMATION_CALLBACK_MODE_PROCESS_MANUAL);
	s.tree->set("parameters/timescale/scale", 2.0);

	s.tree->advance(0.0);
	s.tree->advance(0.5);

	cleanup_tree_setup(s);
}

// ---------- AnimationTree with TimeSeek ----------

TEST_CASE("[SceneTree][AnimationTree] TimeSeek node seeks to position") {
	HashMap<StringName, Ref<Animation>> anims;
	anims.insert("anim", make_value_animation(2.0));

	TreeSetup s = make_tree_with_player(anims);

	Ref<AnimationNodeAnimation> anim_node;
	anim_node.instantiate();
	anim_node->set_animation("anim");
	s.blend_tree->add_node("anim", anim_node);

	Ref<AnimationNodeTimeSeek> seek;
	seek.instantiate();
	s.blend_tree->add_node("seek", seek);

	s.blend_tree->connect_node("seek", 0, "anim");
	s.blend_tree->connect_node("output", 0, "seek");

	s.tree->set_callback_mode_process(AnimationMixer::ANIMATION_CALLBACK_MODE_PROCESS_MANUAL);

	s.tree->advance(0.0);
	s.tree->set("parameters/seek/seek_request", 1.0);
	s.tree->advance(0.0);

	cleanup_tree_setup(s);
}

// ---------- NodeTimeInfo ----------

TEST_CASE("[AnimationTree] NodeTimeInfo get_remain for non-looping") {
	AnimationNode::NodeTimeInfo info;
	info.length = 2.0;
	info.position = 0.5;
	info.loop_mode = Animation::LOOP_NONE;
	info.is_infinity = false;

	double remain = info.get_remain();
	CHECK(remain == doctest::Approx(1.5));
}

TEST_CASE("[AnimationTree] NodeTimeInfo get_remain for looping returns huge length") {
	AnimationNode::NodeTimeInfo info;
	info.length = 2.0;
	info.position = 0.5;
	info.loop_mode = Animation::LOOP_LINEAR;
	info.is_infinity = false;

	double remain = info.get_remain(false);
	CHECK(remain == doctest::Approx(HUGE_LENGTH));
}

TEST_CASE("[AnimationTree] NodeTimeInfo get_remain with break_loop at end") {
	AnimationNode::NodeTimeInfo info;
	info.length = 2.0;
	info.position = 0.5;
	info.loop_mode = Animation::LOOP_LINEAR;
	info.will_end = true;
	info.is_infinity = false;

	double remain = info.get_remain(true);
	CHECK(remain == doctest::Approx(0.0));
}

TEST_CASE("[AnimationTree] NodeTimeInfo get_remain with infinity") {
	AnimationNode::NodeTimeInfo info;
	info.length = 2.0;
	info.position = 0.5;
	info.loop_mode = Animation::LOOP_NONE;
	info.is_infinity = true;

	double remain = info.get_remain();
	CHECK(remain == doctest::Approx(HUGE_LENGTH));
}

TEST_CASE("[AnimationTree] NodeTimeInfo get_remain at exact end") {
	AnimationNode::NodeTimeInfo info;
	info.length = 2.0;
	info.position = 2.0;
	info.loop_mode = Animation::LOOP_NONE;
	info.is_infinity = false;

	double remain = info.get_remain();
	CHECK(remain == doctest::Approx(0.0));
}

TEST_CASE("[AnimationTree] NodeTimeInfo is_looping") {
	AnimationNode::NodeTimeInfo info;

	info.loop_mode = Animation::LOOP_NONE;
	CHECK_FALSE(info.is_looping());

	info.loop_mode = Animation::LOOP_LINEAR;
	CHECK(info.is_looping());

	info.loop_mode = Animation::LOOP_PINGPONG;
	CHECK(info.is_looping());
}

} // namespace TestAnimationTree
