extends Object

## Helper that builds a SpriteFrames resource from horizontal sprite sheets.
## All animations live in `assets/sprites/<entity>/<anim>.png` as a strip of
## frames laid out left-to-right, all the same width/height.

static func build(specs: Array) -> SpriteFrames:
	var frames := SpriteFrames.new()
	if frames.has_animation(&"default"):
		frames.remove_animation(&"default")
	for s in specs:
		var anim_name := StringName(s["name"])
		frames.add_animation(anim_name)
		frames.set_animation_loop(anim_name, bool(s.get("loop", true)))
		frames.set_animation_speed(anim_name, float(s.get("fps", 6.0)))
		var tex: Texture2D = load(s["path"])
		var w: int = int(s["w"])
		var h: int = int(s["h"])
		for i in range(int(s["count"])):
			var atlas := AtlasTexture.new()
			atlas.atlas = tex
			atlas.region = Rect2(i * w, 0, w, h)
			frames.add_frame(anim_name, atlas)
	return frames
